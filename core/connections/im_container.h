#pragma once

#include <memory>

namespace voip_manager {
    struct VoipProxySettings;
    class VoipManager;
}

namespace core
{
    struct proxy_settings;
    class base_im;
    class login_info;
    class phone_info;
    class coll_helper;
    class im_login_list;
    class im_login_id;
    typedef std::vector<std::shared_ptr<base_im>> ims_list;

    namespace  memory_stats {
        class memory_stats_collector;
    }

    using message_function = std::function<void(int64_t, coll_helper&)>;

    #define REGISTER_IM_MESSAGE(_message_string, _callback)                                             \
        messages_map_.emplace(                                                                          \
            _message_string,                                                                            \
            std::bind(&im_container::_callback, this, std::placeholders::_1, std::placeholders::_2));

    class im_container : public std::enable_shared_from_this<im_container>
    {
        std::unordered_map<std::string_view, message_function> messages_map_;

        std::unique_ptr<im_login_list> logins_;
        ims_list ims_;

        std::shared_ptr<voip_manager::VoipManager> voip_manager_;
        std::shared_ptr<memory_stats::memory_stats_collector> memory_stats_collector_;

        bool create_ims();
        void connect_ims();

        // gui message handlers
        void on_login_by_password(int64_t _seq, coll_helper& _params);
        void on_login_by_password_for_attach_uin(int64_t _seq, coll_helper& _params);

        void on_login_get_sms_code(int64_t _seq, coll_helper& _params);
        void on_login_by_phone(int64_t _seq, coll_helper& _params);
        void on_logout(int64_t _seq, coll_helper& _params);
        void on_phoneinfo(int64_t _seq, coll_helper& _params);
        void on_connect_after_migration(int64_t _seq, coll_helper& _params);
        void on_get_contact_avatar(int64_t _seq, coll_helper& _params);
        void on_show_contact_avatar(int64_t _seq, coll_helper& _params);
        void on_remove_contact_avatars(int64_t _seq, coll_helper& _params);
        void on_send_message(int64_t _seq, coll_helper& _params);
        void on_update_message(int64_t _seq, coll_helper& _params);
        void on_message_typing(int64_t _seq, coll_helper& _params);
        void on_feedback(int64_t _seq, coll_helper& _params);
        void on_set_state(int64_t _seq, coll_helper& _params);

        void on_dialogs_search_local(int64_t _seq, coll_helper& _params);
        void on_dialogs_search_local_ended(int64_t _seq, coll_helper& _params);
        void on_dialogs_search_server(int64_t _seq, coll_helper& _params);

        void on_dialogs_search_add_pattern(int64_t _seq, coll_helper& _params);
        void on_dialogs_search_remove_pattern(int64_t _seq, coll_helper& _params);

        void on_get_archive_messages_buddies(int64_t _seq, coll_helper& _params);
        void on_get_archive_messages(int64_t _seq, coll_helper& _params);
        void on_get_archive_mentions(int64_t _seq, coll_helper& _params);
        void on_delete_archive_messages(int64_t _seq, coll_helper& _params);
        void on_delete_archive_messages_from(int64_t _seq, coll_helper& _params);
        void on_delete_archive_all_messages(int64_t _seq, coll_helper& _params);
        void on_get_message_context(int64_t _seq, coll_helper& _params);
        void on_archive_log_model(int64_t _seq, coll_helper& _params);
        void on_add_opened_dialog(int64_t _seq, coll_helper& _params);
        void on_remove_opened_dialog(int64_t _seq, coll_helper& _params);
        void on_set_last_read(int64_t _seq, coll_helper& _params);
        void on_set_last_read_mention(int64_t _seq, coll_helper& _params);
        void on_set_last_read_partial(int64_t _seq, coll_helper& _params);
        void on_hide_chat(int64_t _seq, coll_helper& _params);
        void on_mute_chat(int64_t _seq, coll_helper& _params);
        void on_upload_file_sharing(int64_t _seq, coll_helper& _params);
        void on_abort_file_sharing_uploading(int64_t _seq, coll_helper& _params);

        void on_get_file_sharing_preview_size(int64_t _seq, coll_helper& _params);
        void on_download_file_sharing_metainfo(int64_t _seq, coll_helper& _params);
        void on_download_file(int64_t _seq, coll_helper& _params);

        void on_download_image(int64_t _seq, coll_helper& _params);
        void on_cancel_loader_task(int64_t _seq, coll_helper& _params);
        void on_download_link_preview(int64_t _seq, coll_helper& _params);
        void on_download_raise_priority(int64_t _seq, coll_helper& _params);
        void on_contact_switched(int64_t _seq, coll_helper& _params);
        void on_abort_file_downloading(int64_t _seq, coll_helper& _params);
        void on_get_stickers_meta(int64_t _seq, coll_helper& _params);

        void on_check_theme_meta_updates(int64_t _seq, coll_helper& _params);
        void on_get_theme_wallpaper(int64_t _seq, coll_helper& _params);
        void on_get_theme_wallpaper_preview(int64_t _seq, coll_helper& _params);
        void on_set_user_wallpaper(int64_t _seq, coll_helper& _params);
        void on_remove_user_wallpaper(int64_t _seq, coll_helper& _params);

        void on_get_sticker(int64_t _seq, coll_helper& _params);
        void on_get_fs_stickers_by_ids(int64_t _seq, coll_helper& _params);
        void on_get_stickers_pack_info(int64_t _seq, coll_helper& _params);
        void on_add_stickers_pack(int64_t _seq, coll_helper& _params);
        void on_remove_stickers_pack(int64_t _seq, coll_helper& _params);
        void on_get_stickers_store(int64_t _seq, coll_helper& _params);
        void on_search_stickers_store(int64_t _seq, coll_helper& _params);
        void on_get_set_icon_big(int64_t _seq, coll_helper& _params);
        void on_clean_set_icon_big(int64_t _seq, coll_helper& _params);
        void on_set_sticker_order(int64_t _seq, coll_helper& _params);
        void on_get_chat_info(int64_t _seq, coll_helper& _params);
        void on_get_chat_home(int64_t _seq, coll_helper& _params);
        void on_resolve_pending(int64_t _seq, coll_helper& _params);
        void on_get_chat_member_info(int64_t _seq, coll_helper& _params);
        void on_get_chat_members(int64_t _seq, coll_helper& _params);
        void on_search_chat_members(int64_t _seq, coll_helper& _params);
        void on_get_chat_contacts(int64_t _seq, coll_helper& _params);

        void on_search_contacts_server(int64_t _seq, coll_helper& _params);
        void on_search_contacts_local(int64_t _seq, coll_helper& _params);

        void on_syncronize_addressbook(int64_t _seq, coll_helper& _params);

        void on_profile(int64_t _seq, coll_helper& _params);
        void on_hide_dlg_state(int64_t _seq, coll_helper& _params);
        void on_set_attention_attribute(int64_t _seq, coll_helper& _params);
        void on_add_contact(int64_t _seq, coll_helper& _params);
        void on_remove_contact(int64_t _seq, coll_helper& _params);
        void on_rename_contact(int64_t _seq, coll_helper& _params);
        void on_report_contact(int64_t _seq, coll_helper& _params);
        void on_url_played(int64_t _seq, coll_helper& _params);
        void on_speech_to_text(int64_t _seq, coll_helper& _params);
        void on_ignore_contact(int64_t _seq, coll_helper& _params);
        void on_get_ignore_contacts(int64_t _seq, coll_helper& _params);
        void on_favorite(int64_t _seq, coll_helper& _params);
        void on_unfavorite(int64_t _seq, coll_helper& _params);

        void on_create_chat(int64_t _seq, coll_helper& _params);

        void on_mod_chat_params(int64_t _seq, coll_helper& _params);
        void on_mod_chat_name(int64_t _seq, coll_helper& _params);
        void on_mod_chat_about(int64_t _seq, coll_helper& _params);
        void on_mod_chat_rules(int64_t _seq, coll_helper& _params);
        void on_mod_chat_public(int64_t _seq, coll_helper& _params);
        void on_mod_chat_join(int64_t _seq, coll_helper& _params);
        void on_mod_chat_link(int64_t _seq, coll_helper& _params);
        void on_mod_chat_ro(int64_t _seq, coll_helper& _params);

        void on_block_chat_member(int64_t _seq, coll_helper& _params);
        void on_set_chat_member_role(int64_t _seq, coll_helper& _params);
        void on_chat_pin_message(int64_t _seq, coll_helper& _params);

        void on_set_user_proxy(int64_t _seq, coll_helper& _params);
        void on_join_livechat(int64_t _seq, coll_helper& _params);
        void on_set_locale(int64_t _seq, coll_helper& _params);
        void on_set_avatar(int64_t _seq, coll_helper& _params);

        void on_voip_call_message(int64_t _seq, coll_helper& _params);
        void on_voip_avatar_msg(std::shared_ptr<base_im> im, coll_helper& _params);
        void on_voip_background_msg(std::shared_ptr<base_im> im, coll_helper& _params);

        // group chat
        void on_remove_members(int64_t _seq, coll_helper& _params);
        void on_add_members(int64_t _seq, coll_helper& _params);
        void on_add_chat(int64_t _seq, coll_helper& _params);
        void on_modify_chat(int64_t _seq, coll_helper& _params);

        //mrim
        void on_mrim_get_key(int64_t _seq, coll_helper& _params);

        // tools
        void on_sign_url(int64_t _seq, coll_helper& _params);
        void on_stats(int64_t _seq, coll_helper& _params);
        void on_im_stats(int64_t _seq, coll_helper& _params);

        std::shared_ptr<base_im> get_im(coll_helper& _params) const;

        void on_update_profile(int64_t _seq, coll_helper& _params);

        void fromInternalProxySettings2Voip(const core::proxy_settings& proxySettings, voip_manager::VoipProxySettings& voipProxySettings);

        // masks
        void on_get_mask_id_list(int64_t _seq, coll_helper& _params);
        void on_get_mask_preview(int64_t _seq, coll_helper& _params);
        void on_get_mask_model(int64_t _seq, coll_helper& _params);
        void on_get_mask(int64_t _seq, coll_helper& _params);
        void on_get_existent_masks(int64_t _seq, coll_helper& _params);

        void on_merge_account(int64_t _seq, coll_helper& _params);

        void on_get_mentions_suggests(int64_t _seq, coll_helper& _params);

        void on_get_code_by_phone_call(int64_t _seq, coll_helper& _params);

        void on_get_attach_phone_info(int64_t _seq, coll_helper& _params);

        void on_get_logs_path(int64_t _seq, coll_helper& _params);
        void on_change_app_config(const int64_t _seq, coll_helper& _params);
        void on_remove_content_cache(const int64_t _seq, coll_helper& _params);
        void on_clear_avatars(const int64_t _seq, coll_helper& _params);

        void on_report_stickerpack(const int64_t _seq, coll_helper& _params);
        void on_report_sticker(const int64_t _seq, coll_helper& _params);
        void on_report_message(const int64_t _seq, coll_helper& _params);

        void on_user_accept_gdpr(const int64_t _seq, coll_helper& _params);

        void set_user_has_been_logged_in_ever(bool _hasEverLoggedIn);

        void on_get_voip_calls_quality_popup_conf(const int64_t _seq, coll_helper& _params);
        void on_send_voip_calls_quality_report(const int64_t _seq, coll_helper& _params);
        void on_get_dialog_gallery(const int64_t _seq, coll_helper& _params);
        void on_get_dialog_gallery_by_msg(const int64_t _seq, coll_helper& _params);
        void on_request_gallery_state(const int64_t _seq, coll_helper& _params);
        void on_get_gallery_index(const int64_t _seq, coll_helper& _params);
        void on_make_gallery_hole(const int64_t _seq, coll_helper& _params);

        void on_request_memory_usage(const int64_t _seq, coll_helper& _params);
        void on_report_memory_usage(const int64_t _seq, coll_helper& _params);
        void on_get_ram_usage(const int64_t _seq, coll_helper& _params);

        void on_make_archive_holes(const int64_t _seq, coll_helper& _params);
        void on_invalidate_archive_data(const int64_t _seq, coll_helper& _params);

        void on_ui_activity(const int64_t _seq, coll_helper& _params);

        void on_close_stranger(const int64_t _seq, coll_helper& _params);

        void on_local_pin_set(const int64_t _seq, coll_helper& _params);
        void on_local_pin_entered(const int64_t _seq, coll_helper& _params);
        void on_local_pin_disable(const int64_t _seq, coll_helper& _params);

        void on_update_check(const int64_t _seq, coll_helper& _params);

        void on_get_id_info(const int64_t _seq, coll_helper& _params);
        void on_get_user_info(const int64_t _seq, coll_helper& _params);
        void on_get_user_last_seen(const int64_t _seq, coll_helper& _params);

        void on_set_privacy_settings(const int64_t _seq, coll_helper& _params);
        void on_get_privacy_settings(const int64_t _seq, coll_helper& _params);

        void on_nickname_check(const int64_t _seq, coll_helper& _params);
        void on_get_common_chats(const int64_t _seq, coll_helper& _params);

        void on_reset_connection(const int64_t _seq, coll_helper& _params);

        void on_update_recent_avatars_size(const int64_t _seq, coll_helper& _params);

    public:

        void on_message_from_gui(std::string_view _message, int64_t _seq, coll_helper& _params);
        std::shared_ptr<base_im> get_im_by_id(int32_t _id) const;
        bool update_login(im_login_id& _login);
        void replace_uin_in_login(im_login_id& old_login, im_login_id& new_login);
        void logout(const bool _is_auth_error);

        bool has_valid_login() const;

        im_container(const std::shared_ptr<voip_manager::VoipManager>& voip_manager,
                     const std::shared_ptr<memory_stats::memory_stats_collector>& memory_stats_collector);
        virtual ~im_container();

        void create();
        std::string get_first_login() const;
    };
}
