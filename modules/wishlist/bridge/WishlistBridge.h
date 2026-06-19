#pragma once

#include <memory>
#include <string>

namespace mbb
{
class TelegramOps;
class Storage;
class IModule;
class ModuleOps;
} // namespace mbb

namespace wishlist
{

// Factory: creates an IModule backed by Rust WishlistModule
[[nodiscard]] std::unique_ptr<mbb::IModule> CreateWishlistModule(std::shared_ptr<mbb::TelegramOps> ops,
                                                                 std::shared_ptr<mbb::Storage> storage,
                                                                 const std::string& dbPath);

} // namespace wishlist
