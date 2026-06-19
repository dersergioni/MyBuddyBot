use crate::dispatch;
use crate::module::create_wishlist_module;
use crate::session::{InputState, SessionKey};
use crate::{FakeModuleOps, RecordedOp, WishlistModule};
use wishlist_core::families::CreateFamilyResult;

fn unique_db_path(test_name: &str) -> String {
    std::env::temp_dir()
        .join(format!(
            "wishlist-telegram-{test_name}-{}.db",
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .expect("system time should be after unix epoch")
                .as_nanos()
        ))
        .to_string_lossy()
        .into_owned()
}

fn create_test_module(test_name: &str) -> Box<WishlistModule> {
    create_wishlist_module(&unique_db_path(test_name)).expect("wishlist module should initialize")
}

fn first_sent_message_id(operations: &[RecordedOp]) -> i32 {
    operations
        .iter()
        .find_map(|op| match op {
            RecordedOp::SendWithInlineButtons { message_id, .. } => Some(*message_id),
            _ => None,
        })
        .expect("expected send_with_inline_buttons operation")
}

fn owner_list_id(module: &WishlistModule, owner_id: i64) -> i64 {
    module
        .service
        .lists()
        .list_owner_lists(owner_id)
        .expect("owner lists should load")
        .into_iter()
        .next()
        .expect("owner should have a list")
        .id
}

fn owner_wish_id(module: &WishlistModule, list_id: i64) -> i64 {
    module
        .service
        .wishes()
        .list_wishes(list_id)
        .expect("wishes should load")
        .into_iter()
        .next()
        .expect("list should contain a wish")
        .id
}

fn owner_family_id(module: &WishlistModule, owner_id: i64) -> i64 {
    module
        .service
        .families()
        .get_family_for_user(owner_id)
        .expect("family lookup should succeed")
        .expect("owner should belong to a family")
        .id
}

#[test]
fn create_list_flow_retries_after_validation_error_then_succeeds() {
    let module = create_test_module("create-list-retry");
    let ops = FakeModuleOps::default();
    let chat_id = 101;
    let thread_id = 7;
    let owner_id = 42;

    dispatch::handle_trigger(&module, &ops, chat_id, thread_id, owner_id, "alice", 0);
    let trigger_ops = ops.take_operations();
    let menu_message_id = first_sent_message_id(&trigger_ops);

    dispatch::handle_callback(
        &module,
        &ops,
        chat_id,
        thread_id,
        menu_message_id,
        owner_id,
        "wl:list:create",
        "cb-create-list",
    );
    let prompt_ops = ops.take_operations();

    assert!(matches!(
        &prompt_ops[0],
        RecordedOp::AnswerCallback {
            callback_query_id,
            text
        } if callback_query_id == "cb-create-list" && text.is_empty()
    ));
    assert!(matches!(
        &prompt_ops[1],
        RecordedOp::EditWithInlineButtons { text, .. }
            if text == "📝 Enter the name for your new list:"
    ));

    assert!(dispatch::handle_text_input(
        &module, &ops, chat_id, thread_id, owner_id, "   ", 0
    ));
    let retry_ops = ops.take_operations();

    assert!(retry_ops.iter().any(|op| matches!(
        op,
        RecordedOp::DeleteMessage {
            chat_id: deleted_chat,
            message_id
        } if *deleted_chat == chat_id && *message_id == menu_message_id
    )));
    assert!(retry_ops.iter().any(|op| matches!(
        op,
        RecordedOp::SendWithInlineButtons {
            chat_id: sent_chat,
            thread_id: sent_thread,
            text,
            ..
        } if *sent_chat == chat_id
            && *sent_thread == thread_id
            && text == "❌ Name cannot be empty. Try again:"
    )));

    assert!(dispatch::handle_text_input(
        &module, &ops, chat_id, thread_id, owner_id, "Birthday", 0
    ));
    let success_ops = ops.take_operations();

    assert!(success_ops.iter().any(|op| matches!(
        op,
        RecordedOp::SendWithInlineButtons {
            chat_id: sent_chat,
            thread_id: sent_thread,
            text,
            ..
        } if *sent_chat == chat_id
            && *sent_thread == thread_id
            && text == "✅ List \"Birthday\" created!"
    )));

    let lists = module
        .service
        .lists()
        .list_owner_lists(owner_id)
        .expect("lists should load");
    assert_eq!(lists.len(), 1);
    assert_eq!(lists[0].name, "Birthday");
}

#[test]
fn add_wish_flow_creates_wish_and_moves_to_priority_and_url_steps() {
    let module = create_test_module("add-wish-flow");
    let ops = FakeModuleOps::default();
    let chat_id = 201;
    let thread_id = 3;
    let owner_id = 7;
    let key = SessionKey::new(chat_id, thread_id, owner_id);

    module
        .service
        .lists()
        .create_list(owner_id, "Birthday", false)
        .expect("list should be created");
    let list_id = owner_list_id(&module, owner_id);

    module.sessions.touch(key);

    dispatch::handle_callback(
        &module,
        &ops,
        chat_id,
        thread_id,
        55,
        owner_id,
        &format!("wl:wish:add-from-list:{list_id}"),
        "cb-add-wish",
    );
    let prompt_ops = ops.take_operations();

    assert!(matches!(
        &prompt_ops[0],
        RecordedOp::AnswerCallback {
            callback_query_id,
            text
        } if callback_query_id == "cb-add-wish" && text.is_empty()
    ));
    assert!(matches!(
        &prompt_ops[1],
        RecordedOp::EditWithInlineButtons { text, .. }
            if text == "📝 Enter the title for your new wish:"
    ));

    assert!(dispatch::handle_text_input(
        &module,
        &ops,
        chat_id,
        thread_id,
        owner_id,
        "Steam Deck",
        0
    ));
    let created_ops = ops.take_operations();
    let priority_message_id = first_sent_message_id(&created_ops);

    assert!(created_ops.iter().any(|op| matches!(
        op,
        RecordedOp::DeleteMessage {
            chat_id: deleted_chat,
            message_id
        } if *deleted_chat == chat_id && *message_id == 55
    )));
    assert!(created_ops.iter().any(|op| matches!(
        op,
        RecordedOp::SendWithInlineButtons {
            text,
            message_id,
            ..
        } if *message_id == priority_message_id
            && text == "⭐ Choose a priority for your new wish, or continue without one:"
    )));

    let wish_id = owner_wish_id(&module, list_id);

    dispatch::handle_callback(
        &module,
        &ops,
        chat_id,
        thread_id,
        priority_message_id,
        owner_id,
        &format!("wl:wish:create:prio:{list_id}:{wish_id}:2"),
        "cb-create-priority",
    );
    let priority_ops = ops.take_operations();

    assert!(priority_ops.iter().any(|op| matches!(
        op,
        RecordedOp::EditWithInlineButtons { text, .. }
            if text == "🔗 Enter a URL for this wish, or choose an option below:"
    )));

    let wish = module
        .service
        .wishes()
        .get_owner_wish(owner_id, list_id, wish_id)
        .expect("wish lookup should succeed")
        .expect("wish should exist");
    assert_eq!(wish.priority, Some(2));
    assert!(matches!(
        module.sessions.take_input_state(key),
        Some(InputState::WishCreateUrl {
            list_id: stored_list_id,
            wish_id: stored_wish_id,
        }) if stored_list_id == list_id && stored_wish_id == wish_id
    ));
}

#[test]
fn family_invitation_callbacks_cover_accept_and_decline_without_active_session() {
    let module = create_test_module("family-invite-callbacks");
    let ops = FakeModuleOps::default();
    let owner_id = 7;

    let created = module
        .service
        .families()
        .create_family(owner_id, "Weekend Crew")
        .expect("family creation should succeed");
    assert!(matches!(created, CreateFamilyResult::Created(_)));
    let family_id = owner_family_id(&module, owner_id);

    module
        .service
        .families()
        .invite_member_by_owner(owner_id, family_id, 11)
        .expect("invitation should succeed");
    module
        .service
        .families()
        .invite_member_by_owner(owner_id, family_id, 12)
        .expect("second invitation should succeed");

    dispatch::handle_callback(
        &module,
        &ops,
        11,
        0,
        301,
        11,
        &format!("wl:fam:accept:{family_id}"),
        "cb-accept-family",
    );
    let accept_ops = ops.take_operations();

    assert!(accept_ops.iter().any(|op| matches!(
        op,
        RecordedOp::EditText {
            chat_id,
            message_id,
            text
        } if *chat_id == 11 && *message_id == 301 && text == "✅ You joined the family!"
    )));
    assert!(
        module
            .service
            .families()
            .get_family_for_user(11)
            .expect("family lookup should succeed")
            .is_some()
    );

    dispatch::handle_callback(
        &module,
        &ops,
        12,
        0,
        302,
        12,
        &format!("wl:fam:decline:{family_id}"),
        "cb-decline-family",
    );
    let decline_ops = ops.take_operations();

    assert!(decline_ops.iter().any(|op| matches!(
        op,
        RecordedOp::EditText {
            chat_id,
            message_id,
            text
        } if *chat_id == 12 && *message_id == 302 && text == "✅ Invitation declined."
    )));
    assert!(
        module
            .service
            .families()
            .get_pending_invitation(12)
            .expect("pending invitation lookup should succeed")
            .is_none()
    );
}

#[test]
fn family_reserve_and_unreserve_flow_updates_wish_and_notifies_owner() {
    let module = create_test_module("family-reserve-flow");
    let ops = FakeModuleOps::default();
    let owner_id = 7;
    let member_id = 11;
    let member_chat_id = 811;

    let created = module
        .service
        .families()
        .create_family(owner_id, "Gift Crew")
        .expect("family creation should succeed");
    assert!(matches!(created, CreateFamilyResult::Created(_)));
    let family_id = owner_family_id(&module, owner_id);

    module
        .service
        .families()
        .invite_member_by_owner(owner_id, family_id, member_id)
        .expect("invitation should succeed");
    module
        .service
        .families()
        .accept_invitation_checked(member_id, family_id)
        .expect("acceptance should succeed");

    let list_id = module
        .service
        .lists()
        .create_list(owner_id, "Birthday", false)
        .expect("list should be created")
        .id;
    let wish_id = module
        .service
        .wishes()
        .create_wish(owner_id, list_id, "Camera")
        .expect("wish should be created");

    module
        .sessions
        .touch(SessionKey::new(member_chat_id, 0, member_id));

    dispatch::handle_callback(
        &module,
        &ops,
        member_chat_id,
        0,
        401,
        member_id,
        &format!("wl:fam:reserve:{owner_id}:{list_id}:{wish_id}"),
        "cb-reserve",
    );
    let reserve_ops = ops.take_operations();

    assert!(reserve_ops.iter().any(|op| matches!(
        op,
        RecordedOp::SendText {
            chat_id,
            thread_id,
            text
        } if *chat_id == owner_id
            && *thread_id == 0
            && text.contains("Someone just reserved one of your wishlist gifts")
    )));
    let reserved_wish = module
        .service
        .wishes()
        .get_wish(list_id, wish_id)
        .expect("wish lookup should succeed")
        .expect("wish should exist");
    assert_eq!(reserved_wish.reserved_by, Some(member_id));

    dispatch::handle_callback(
        &module,
        &ops,
        member_chat_id,
        0,
        401,
        member_id,
        &format!("wl:fam:unreserve:{owner_id}:{list_id}:{wish_id}"),
        "cb-unreserve",
    );
    let unreserve_ops = ops.take_operations();

    assert!(!unreserve_ops.iter().any(|op| matches!(
        op,
        RecordedOp::SendText { chat_id, .. } if *chat_id == owner_id
    )));
    let unreserved_wish = module
        .service
        .wishes()
        .get_wish(list_id, wish_id)
        .expect("wish lookup should succeed")
        .expect("wish should exist");
    assert_eq!(unreserved_wish.reserved_by, None);
}
