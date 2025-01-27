#include "stdafx.h"
#include "wim_im.h"
#include "wim_packet.h"
#include "../contact_profile.h"

// packets
#include "packets/client_login.h"
#include "packets/start_session.h"
#include "packets/fetch.h"
#include "packets/get_sms_code.h"
#include "packets/get_chat_info.h"
#include "packets/get_chat_home.h"
#include "packets/login_by_phone.h"
#include "packets/normalize_phone.h"
#include "packets/send_message.h"
#include "packets/get_history.h"
#include "packets/get_history_batch.h"
#include "packets/set_dlg_state.h"
#include "packets/wim_webrtc.h"
#include "packets/hide_chat.h"
#include "packets/get_stickers_index.h"
#include "packets/get_stickers_store.h"
#include "packets/get_stickers_pack_info.h"
#include "packets/add_stickers_pack.h"
#include "packets/remove_stickers_pack.h"
#include "packets/get_themes_index.h"
#include "packets/get_profile.h"
#include "packets/search_contacts.h"
#include "packets/search_dialogs.h"
#include "packets/add_buddy.h"
#include "packets/remove_buddy.h"
#include "packets/set_buddy_attribute.h"
#include "packets/mute_buddy.h"
#include "packets/remove_members.h"
#include "packets/add_members.h"
#include "packets/add_chat.h"
#include "packets/mrim_get_key.h"
#include "packets/modify_chat.h"
#include "packets/set_permit_deny.h"
#include "packets/get_permit_deny.h"
#include "packets/set_state.h"
#include "packets/end_session.h"
#include "packets/send_message_typing.h"
#include "packets/send_feedback.h"
#include "packets/speech_to_text.h"
#include "packets/del_message.h"
#include "packets/del_history.h"
#include "packets/attach_phone.h"
#include "packets/attach_uin.h"
#include "packets/merge_account.h"
#include "packets/update_profile.h"
#include "packets/join_chat_alpha.h"
#include "packets/set_timezone.h"
#include "packets/set_avatar.h"
#include "packets/mod_chat_alpha.h"
#include "packets/block_chat_member.h"
#include "packets/mod_chat_member_alpha.h"
#include "packets/phoneinfo.h"
#include "packets/resolve_pending.h"
#include "packets/create_chat.h"
#include "packets/pin_message.h"
#include "packets/set_stickers_order.h"
#include "packets/get_mentions_suggests.h"
#include "packets/get_attach_phone_info.h"
#include "packets/set_attention_attribute.h"
#include "packets/report_abuse.h"
#include "packets/gdpr_agreement.h"
#include "packets/get_voip_calls_quality_popup_conf.h"
#include "packets/send_stat.h"
#include "packets/get_dialog_gallery.h"
#include "packets/stickers_migration.h"
#include "packets/get_user_info.h"
#include "packets/get_id_info.h"
#include "packets/get_privacy_settings.h"
#include "packets/set_privacy_settings.h"
#include "packets/get_user_last_seen.h"
#include "packets/check_nick.h"
#include "packets/get_common_chats.h"
#include "packets/sync_ab.h"
#include "packets/get_chat_member_info.h"
#include "packets/get_chat_members.h"
#include "packets/get_chat_contacts.h"
#include "packets/search_chat_members.h"

#include "../../http_request.h"
#include "../../network_log.h"
#include "../../curl_handler.h"

//events
#include "events/fetch_event.h"
#include "events/fetch_event_buddy_list.h"
#include "events/fetch_event_presence.h"
#include "events/fetch_event_dlg_state.h"
#include "events/fetch_event_hidden_chat.h"
#include "events/fetch_event_diff.h"
#include "events/fetch_event_my_info.h"
#include "events/fetch_event_typing.h"
#include "events/fetch_event_permit.h"
#include "events/fetch_event_imstate.h"
#include "events/fetch_event_notification.h"
#include "events/fetch_event_appsdata.h"
#include "events/fetch_event_mention_me.h"
#include "events/fetch_event_chat_heads.h"
#include "events/fetch_event_gallery_notify.h"
#include "events/fetch_event_mchat.h"

//avatars
#include "avatar_loader.h"

// cashed contactlist objects
#include "wim_contactlist_cache.h"
#include "active_dialogs.h"
#include "favorites.h"
#include "mailboxes.h"

#include "../../async_task.h"

#include "async_loader/async_loader.h"

#include "loader/loader.h"
#include "../../../common.shared/loader_errors.h"
#include "loader/loader_handlers.h"
#include "loader/web_file_info.h"
#include "loader/preview_proxy.h"

// stickers
#include "../../stickers/stickers.h"

// masks
#include "../../masks/masks.h"

#include "auth_parameters.h"
#include "../../themes/wallpaper_loader.h"
#include "../../core.h"
#include "../../../corelib/core_face.h"
#include "../../../corelib/enumerations.h"
#include "../../utils.h"
#include "../login_info.h"
#include "../im_login.h"
#include "../../archive/local_history.h"
#include "../../archive/contact_archive.h"
#include "../../archive/history_message.h"
#include "../../archive/archive_index.h"
#include "../../archive/not_sent_messages.h"
#include "../../archive/messages_data.h"
#include "stat/imstat.h"
#include "dialog_holes.h"

#include "../../log/log.h"

#include "../../../common.shared/url_parser/url_parser.h"
#include "../../../common.shared/version_info.h"

#include "../../tools/system.h"
#include "../../tools/file_sharing.h"
#include "../../tools/json_helper.h"
#include "tools/device_id.h"

// config
#include "../../configuration/app_config.h"

#include "../../search_pattern_history.h"
#include "stats/memory/memory_stats_collector.h"

#include "utils.h"

#include "permit_info.h"

#include "chat_params.h"

#include "wim_im.h"
#include "../urls_cache.h"

#include "../../../libomicron/include/omicron/omicron.h"

using namespace core;
using namespace wim;

namespace
{
    constexpr auto send_message_timeout = std::chrono::milliseconds(500);
    constexpr auto start_session_timeout = std::chrono::seconds(10);
    constexpr uint32_t sent_repeat_count = 2;
    constexpr int64_t prefetch_count = 30;
    constexpr int32_t empty_timer_id = -1;
    constexpr auto post_messages_rate = std::chrono::seconds(1);
    constexpr auto rate_limit_timeout = std::chrono::seconds(30);
    constexpr auto robusto_rate_limit_timeout = std::chrono::seconds(10);
    constexpr auto dlg_state_agregate_start_timeout = std::chrono::minutes(3);
    constexpr auto dlg_state_agregate_period = std::chrono::minutes(1);
    constexpr int32_t max_fetch_events_count = 20;
    constexpr int32_t msg_context_size = 200;
    constexpr auto send_stat_interval = std::chrono::hours(24);
    constexpr size_t contacts_per_search_thread = 100;
    constexpr std::string_view default_patch_version("none");
    constexpr int64_t initial_request_avatar_seq = -1;
}

std::string& get_strickers_size()
{
    static std::string stickers_size("small");

    return stickers_size;
}

void set_stickers_size(const std::string& _size)
{
    auto& stickers_size = get_strickers_size();

    stickers_size = _size;
}

void write_offset_in_log(time_t offset)
{
    tools::binary_stream bs;
    bs.write<std::string_view>("new offset is ");
    std::stringstream s;
    s << offset;
    bs.write<std::string>(s.str());
    bs.write<std::string_view>("\r\n");

    g_core->write_data_to_network_log(std::move(bs));
}

void write_ignore_download_holes(std::string_view contact, std::string_view reason)
{
    tools::binary_stream bs;
    bs.write<std::string_view>("ignore download holes for ");
    bs.write<std::string_view>(contact);
    bs.write<std::string_view>(": ");
    bs.write<std::string_view>(reason);
    bs.write<std::string_view>("\r\n");

    g_core->write_data_to_network_log(std::move(bs));
}

void write_time_offests_in_log(const std::shared_ptr<fetch>& packet)
{
    std::stringstream s;
    s << "fecth packet: start execute: " << packet->get_start_execute_time()
        << "; now: " << packet->now() << "; request_time: " << packet->get_request_time()
        << "; timezone_offset: " << packet->get_timezone_offset()
        << "; time_offset: " << packet->get_time_offset()
        << "; time_offset_local: " << packet->get_time_offset_local();
    s << "\r\n";

    tools::binary_stream bs;
    bs.write<std::string>(s.str());

    g_core->write_data_to_network_log(std::move(bs));
}

void write_files_error_in_log(std::string_view _func, int32_t _error)
{
    tools::binary_stream bs;
    bs.write<std::string_view>(_func);
    std::stringstream s;
    s << " error is ";
    s << _error;
    bs.write<std::string>(s.str());
    bs.write<std::string_view>("\r\n");

    g_core->write_data_to_network_log(std::move(bs));
}

void write_files_error_in_log(std::string_view _func, const archive::error_vector& _errors)
{
    tools::binary_stream bs;
    bs.write<std::string_view>(_func);
    bs.write<std::string_view>(" errors: ");
    std::stringstream s;
    for (auto [id, error] : _errors)
        s << "id: " << id << " error: " << error << "; ";

    bs.write<std::string>(s.str());
    bs.write<std::string_view>("\r\n");

    g_core->write_data_to_network_log(std::move(bs));
}

void search::search_data::reset_post_timer()
{
    if (g_core && post_results_timer_ != empty_timer_id)
        g_core->stop_timer(post_results_timer_);
    post_results_timer_ = empty_timer_id;
}

//////////////////////////////////////////////////////////////////////////
// send_thread class
//////////////////////////////////////////////////////////////////////////
core::wim::wim_send_thread::wim_send_thread()
    : async_executer("wim_send")
    , is_packet_execute_(false)
{
}

core::wim::wim_send_thread::~wim_send_thread()
{
}

std::shared_ptr<async_task_handlers> core::wim::wim_send_thread::post_packet(
    std::shared_ptr<wim_packet> _packet,
    std::function<void(int32_t)> _error_handler,
    std::shared_ptr<async_task_handlers> _handlers)
{
    std::shared_ptr<async_task_handlers> callback_handlers = _handlers ? _handlers : std::make_shared<async_task_handlers>();

    const auto can_run_async = _packet->support_async_execution();

    if (is_packet_execute_ && !can_run_async)
    {
        packets_queue_.push_back(task_and_params(_packet, _error_handler, callback_handlers));
        return callback_handlers;
    }

    if (!can_run_async)
        is_packet_execute_ = true;

    auto wr_this = weak_from_this();
    const auto current_time = std::chrono::system_clock::now();

    // need wait for timeout (ratelimts)
    if (current_time < cancel_packets_time_)
    {
        run_async_function([]()->int32_t
        {
            return 0;

        })->on_result_ = [wr_this, callback_handlers](int32_t /*_error*/)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            if (callback_handlers->on_result_)
                callback_handlers->on_result_(wpie_error_request_canceled_wait_timeout);

            ptr_this->is_packet_execute_ = false;
            ptr_this->execute_packet_from_queue();
        };

        return callback_handlers;
    }

    if (can_run_async)
    {
        _packet->execute_async([wr_this, _error_handler, callback_handlers](int32_t _error)
        {
            g_core->execute_core_context([wr_this, callback_handlers, _error]()
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                if (callback_handlers->on_result_)
                    callback_handlers->on_result_(_error);
            });
        });

        return callback_handlers;
    }

    auto internal_handlers = run_async_function([_packet]()->int32_t
    {
        return _packet->execute();
    });

    internal_handlers->on_result_ = [wr_this, _packet, _error_handler, callback_handlers](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == wim_protocol_internal_error::wpie_network_error)
        {
            if ((_packet->get_repeat_count() < sent_repeat_count) && (!_packet->is_stopped()))
            {
                ptr_this->is_packet_execute_ = false;

                ptr_this->post_packet(_packet, _error_handler, callback_handlers);

                return;
            }
        }

        if (_error == wpie_error_too_fast_sending)
        {
            ptr_this->cancel_packets_time_ = std::chrono::system_clock::now() + rate_limit_timeout;
        }
        else if (_error == wpie_error_robuso_too_fast_sending)
        {
            ptr_this->cancel_packets_time_ = std::chrono::system_clock::now() + robusto_rate_limit_timeout;
        }

        if (callback_handlers->on_result_)
            callback_handlers->on_result_(_error);

        ptr_this->is_packet_execute_ = false;

        if (_error_handler)
        {
            _error_handler(_error);
        }

        ptr_this->execute_packet_from_queue();
    };

    return callback_handlers;
}


std::shared_ptr<async_task_handlers> core::wim::wim_send_thread::post_packet(std::shared_ptr<wim_packet> _packet, std::function<void(int32_t)>&& _error_handler)
{
    return post_packet(_packet, std::move(_error_handler), nullptr);
}


void core::wim::wim_send_thread::execute_packet_from_queue()
{
    if (!packets_queue_.empty())
    {
        auto next_packet = std::move(packets_queue_.front());
        packets_queue_.pop_front();

        post_packet(std::move(next_packet.task_), std::move(next_packet.error_handler_), std::move(next_packet.callback_handlers_));
    }
}

void core::wim::wim_send_thread::clear()
{
    for (const auto& packet : packets_queue_)
        if (packet.callback_handlers_->on_result_)
            packet.callback_handlers_->on_result_(wpie_error_task_canceled);

    packets_queue_.clear();

}
//////////////////////////////////////////////////////////////////////////
// end send_thread class
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<async_task_handlers> remove_messages_from_not_sent(
    const std::shared_ptr<archive::face>& _archive,
    const std::string& _contact,
    const bool _remove_if_modified,
    const std::shared_ptr<archive::history_block>& _messages);

std::shared_ptr<async_task_handlers> remove_messages_from_not_sent(
    const std::shared_ptr<archive::face>& _archive,
    const std::string& _contact,
    const bool _remove_if_modified,
    const std::shared_ptr<archive::history_block>& _tail_messages,
    const std::shared_ptr<archive::history_block>& _intro_messages);

constexpr size_t search_threads_count = 3;
constexpr auto sending_search_results_interval = std::chrono::milliseconds(500);

//////////////////////////////////////////////////////////////////////////
// im class
//////////////////////////////////////////////////////////////////////////
im::im(const im_login_id& _login,
       std::shared_ptr<voip_manager::VoipManager> _voip_manager,
       std::shared_ptr<memory_stats::memory_stats_collector> _memory_stats_collector)
    : base_im(_login, _voip_manager, _memory_stats_collector),
    stop_objects_(std::make_shared<stop_objects>()),
    contact_list_(std::make_shared<contactlist>()),
    active_dialogs_(std::make_shared<active_dialogs>()),
    my_info_cache_(std::make_shared<my_info_cache>()),
    favorites_(std::make_shared<favorites>()),
    mailbox_storage_(std::make_shared<mailbox_storage>()),
    auth_params_(std::make_shared<auth_parameters>()),
    attached_auth_params_(std::make_shared<auth_parameters>()),
    fetch_params_(std::make_shared<fetch_parameters>()),
    wim_send_thread_(std::make_shared<wim_send_thread>()),
    fetch_thread_(std::make_shared<fetch_thread>()),
    async_tasks_(std::make_shared<async_executer>("im async tasks")),

    store_timer_id_(0),
    statistic_timer_id_(0),
    hosts_config_timer_id_(0),
    dlg_state_timer_(0),
    stat_timer_id_(empty_timer_id),
    ui_activity_timer_id_(0),
    im_created_(false),
    failed_holes_requests_(std::make_shared<holes::failed_requests>()),
    sent_pending_messages_active_(false),
    sent_pending_delete_messages_active_(false),
    imstat_(std::make_unique<statistic::imstat>()),
    history_searcher_(nullptr),
    start_session_time_(std::chrono::system_clock::now() - start_session_timeout),
    prefetch_uid_(std::numeric_limits<int64_t>::max()),
    post_messages_timer_(empty_timer_id),
    mute_chats_timer_(0),
    last_success_network_post_(std::chrono::system_clock::now()),
    last_check_alt_scheme_reset_(std::chrono::system_clock::now()),
    dlg_state_agregate_mode_(false),
    last_network_activity_time_(std::chrono::system_clock::now() - dlg_state_agregate_start_timeout),
    chat_heads_timer_(empty_timer_id),
    ui_active_(false),
    dlg_states_count_(0),
    active_connection_state_(connection_state::init),
    resend_gdpr_info_(nullptr),
    suppress_gdpr_resend_(false),
    waiting_for_local_pin_(false)
{
    init_failed_requests();
}


im::~im()
{
    stop_store_timer();
    stop_dlg_state_timer();
    save_cached_objects(force_save::yes);
    cancel_requests();
    stop_waiters();
    stop_statistic_timer();
    stop_stat_timer();
    stop_ui_activity_timer();
}

void im::init_failed_requests()
{
    auto app_conf = core::configuration::get_app_config();
    static std::set<core::configuration::app_config::gdpr_report_to_server_state> need_retry_states = {
        core::configuration::app_config::gdpr_report_to_server_state::failed,
    };

    auto cur_state = static_cast<core::configuration::app_config::gdpr_report_to_server_state>(app_conf.gdpr_agreement_reported_to_server());
    if (app_conf.gdpr_user_has_agreed() && need_retry_states.find(cur_state) != need_retry_states.end())
    {
        // only accept agreement is supported
        set_resend_gdpr_info(std::make_unique<accept_agreement_info>(accept_agreement_info::agreement_action::accept));
    }
}

void im::schedule_store_timer()
{
    auto wr_this = weak_from_this();

    store_timer_id_ = g_core->add_timer([wr_this]
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->save_cached_objects();

    }, std::chrono::seconds(10));
}

void im::stop_store_timer()
{
    if ((store_timer_id_ > 0) && g_core)
    {
        g_core->stop_timer(store_timer_id_);
    }
}


void im::schedule_statistic_timer()
{
    auto wr_this = weak_from_this();

    constexpr auto timeout = std::chrono::seconds(build::is_debug() ? 10 : 60);

    statistic_timer_id_ = g_core->add_timer([wr_this]
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->send_statistic_if_needed();

    }, timeout);
}

void im::stop_statistic_timer()
{
    if ((statistic_timer_id_ > 0) && g_core)
    {
        g_core->stop_timer(statistic_timer_id_);
    }
}

void im::stop_waiters()
{
    stop_objects_->poll_event_.notify();
    stop_objects_->startsession_event_.notify();
}

void im::connect()
{
    const auto is_locked = g_core->get_local_pin_enabled();
    if (!is_locked)
    {
        load_auth_and_fetch_parameters();
    }
    else
    {
        waiting_for_local_pin_ = true;
        load_my_info();
    }
}

core::wim::wim_packet_params im::make_wim_params()
{
    return make_wim_params_general(true);
}

core::wim::wim_packet_params im::make_wim_params_general(bool _is_current_auth_params)
{
    return make_wim_params_from_auth(_is_current_auth_params ? *auth_params_ : *attached_auth_params_);
}


core::wim::wim_packet_params im::make_wim_params_from_auth(const auth_parameters& _auth_params)
{
    assert(g_core->is_core_thread());

    auto active_session = stop_objects_->active_session_id_;

    const std::weak_ptr<stop_objects> wr_stop = stop_objects_;

    auto stop_handler = [wr_stop, active_session]
    {
        auto ptr_stop = wr_stop.lock();
        if (!ptr_stop)
            return true;

        std::lock_guard<std::mutex> lock(ptr_stop->session_mutex_);
        return(active_session != ptr_stop->active_session_id_);
    };

    auto proxy = g_core->get_proxy_settings();

    return wim_packet_params(
        std::move(stop_handler),
        _auth_params.a_token_,
        _auth_params.session_key_,
        _auth_params.dev_id_,
        _auth_params.aimsid_,
        g_core->get_uniq_device_id(),
        _auth_params.aimid_,
        _auth_params.time_offset_,
        _auth_params.time_offset_local_,
        proxy,
        core::configuration::get_app_config().is_full_log_enabled());
}



void im::handle_net_error(int32_t err)
{
    if (err == wpie_error_need_relogin)
    {
        start_session();
    }
    else if (err == wpie_network_error)
    {
        set_connection_state(connection_state::connecting);
    }
    else
    {
        set_connection_state(connection_state::online);
    }
}

std::shared_ptr<async_task_handlers> im::post_wim_packet(std::shared_ptr<wim_packet> _packet)
{
//    assert(!std::dynamic_pointer_cast<robusto_packet>(_packet));

    auto wr_this = weak_from_this();

    return wim_send_thread_->post_packet(_packet, [wr_this, _packet](int32_t err)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->handle_net_error(err);
    });
}

void im::login(int64_t _seq, const login_info& _info)
{
    switch (_info.get_login_type())
    {
    case login_type::lt_login_password:
        {
            login_by_password(_seq, _info.get_login(), _info.get_password(), _info.get_save_auth_data(), true /* start_session */, false, _info.get_token_type());
        }
        break;
    default:
        {
            assert(!"unknown login type");
        }
        break;
    }
}

void im::start_attach_uin(int64_t _seq, const login_info& _info, const wim_packet_params& _from_params)
{
    switch (_info.get_login_type())
    {
    case login_type::lt_login_password:
        {
            login_by_password_and_attach_uin(_seq, _info.get_login(), _info.get_password(), _from_params);
        }
        break;
    default:
        {
            assert(!"unknown login type");
        }
        break;
    }
}

void im::login_by_agent_token(
    const std::string& _login,
    const std::string& _token,
    const std::string& _guid)
{
    auto packet = std::make_shared<client_login>(make_wim_params(), _login, _token);
    packet->set_product_guid_8x(_guid);

    auto wr_this = weak_from_this();

    post_wim_packet(packet)->on_result_ = [wr_this, packet](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == 0)
        {
            ptr_this->auth_params_->a_token_ = packet->get_a_token();
            ptr_this->auth_params_->session_key_ = packet->get_session_key();
            ptr_this->auth_params_->exipired_in_ = packet->get_expired_in();
            ptr_this->auth_params_->time_offset_ = packet->get_time_offset();

            write_offset_in_log(ptr_this->auth_params_->time_offset_);

            ptr_this->auth_params_->serializable_ = true;

            ptr_this->on_login_result(0, 0, true);

            ptr_this->start_session(is_ping::no, is_login::yes);
        }
        else
        {
            ptr_this->on_login_result(0, _error, true);
        }
    };
}

void im::login_by_password(
    int64_t _seq,
    const std::string& login,
    const std::string& password,
    const bool save_auth_data,
    const bool start_session,
    const bool _from_export_login,
    const token_type _token_type)
{
    auto packet = std::make_shared<client_login>(make_wim_params(), login, password, _token_type);

    post_wim_packet(packet)->on_result_ = [wr_this = weak_from_this(), packet, save_auth_data, _seq, _from_export_login, _token_type](int32_t _error)
    {
        std::shared_ptr<wim::im> ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == 0)
        {
            ptr_this->auth_params_->a_token_ = packet->get_a_token();
            ptr_this->auth_params_->session_key_ = packet->get_session_key();
            ptr_this->auth_params_->exipired_in_ = packet->get_expired_in();
            ptr_this->auth_params_->time_offset_ = packet->get_time_offset();

            write_offset_in_log(ptr_this->auth_params_->time_offset_);

            ptr_this->auth_params_->serializable_ = save_auth_data;

            ptr_this->on_login_result(_seq, 0, _from_export_login);

            if (_token_type == token_type::basic)
                ptr_this->start_session(is_ping::no, is_login::yes);
        }
        else
        {
            ptr_this->on_login_result(_seq, _error, _from_export_login);
        }
    };
}

void im::login_by_password_and_attach_uin(int64_t _seq, const std::string& login, const std::string& password, const wim_packet_params& _from_params)
{
    auto packet = std::make_shared<client_login>(make_wim_params(), login, password); // TODO : new_wim_params

    auto wr_this = weak_from_this();

    post_wim_packet(packet)->on_result_ = [wr_this, packet, _from_params, _seq](int32_t _error)
    {
        std::shared_ptr<wim::im> ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == 0)
        {
            auto auth_params = ptr_this->attached_auth_params_;
            auth_params->a_token_ = packet->get_a_token();
            auth_params->session_key_ = packet->get_session_key();
            auth_params->exipired_in_ = packet->get_expired_in();
            auth_params->time_offset_ = packet->get_time_offset();

            write_offset_in_log(auth_params->time_offset_);

            // ptr_this->auth_params_->serializable_ = save_auth_data;

            ptr_this->on_login_result_attach_uin(_seq, 0, *(ptr_this->attached_auth_params_.get()), _from_params);
        }
        else
        {
            ptr_this->on_login_result_attach_uin(_seq, _error, *(ptr_this->attached_auth_params_.get()), _from_params);
        }
    };
}

std::wstring im::get_auth_parameters_filename()
{
    return (get_im_data_path() + L"/key/" + tools::from_utf8(auth_file_name));
}

std::wstring im::get_fetch_parameters_filename()
{
    return (get_im_data_path() + L"/key/" + tools::from_utf8(fetch_url_file_name));
}

std::wstring im::get_auth_parameters_filename_exported()
{
    return (get_im_data_path() + L"/key/" + tools::from_utf8(auth_export_file_name));
}

std::wstring im::get_auth_parameters_filename_merge()
{
    return (get_im_data_path() + L"/key/" + tools::from_utf8(auth_export_file_name_merge));
}

std::wstring im::get_exported_muted_chats_filename()
{
    return (get_im_data_path() + L"/key/" + tools::from_utf8(muted_chats_export_file_name));
}

void im::add_postponed_for_update_dialog(const std::string& _contact)
{
    postponed_for_update_dialogs_.insert(_contact);
}

void im::remove_postponed_for_update_dialog(const std::string& _contact)
{
    if (const auto it = std::as_const(postponed_for_update_dialogs_).find(_contact); it != std::as_const(postponed_for_update_dialogs_).end())
        postponed_for_update_dialogs_.erase(it);
}

void im::process_postponed_for_update_dialog(const std::string& _contact)
{
    if (const auto it = std::as_const(postponed_for_update_dialogs_).find(_contact); it != std::as_const(postponed_for_update_dialogs_).end())
    {
        postponed_for_update_dialogs_.erase(it);
        get_messages_for_update(_contact);
    }
}

void im::erase_auth_data()
{
    auth_params_->clear();

    store_auth_parameters();
}

void im::store_auth_parameters()
{
    if (!auth_params_->serializable_)
        return;

    auto bstream = std::make_shared<core::tools::binary_stream>();
    auth_params_->serialize(*bstream);

    if (g_core->get_local_pin_enabled())
        bstream->encrypt_aes256(local_pin_hash_);

    const std::wstring file_name = get_auth_parameters_filename();

    async_tasks_->run_async_function([bstream, file_name]
    {
        if (!bstream->save_2_file(file_name))
            return -1;

        return 0;
    });
}

void im::save_auth_params_to_file(std::shared_ptr<auth_parameters> auth_params, const std::wstring& _file_name, std::function<void()> _on_result)
{
    rapidjson::Document doc(rapidjson::Type::kObjectType);
    auth_params->serialize(doc, doc.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::string json_string = rapidjson_get_string(buffer);

    if (!json_string.length())
    {
        assert( false );
        return;
    }

    auto wr_this = weak_from_this();
    async_tasks_->run_async_function([_file_name, str = std::move(json_string)]
                                     {
                                         core::tools::binary_stream bstream;
                                         bstream.write<std::string>(str);
                                         if (!bstream.save_2_file(_file_name))
                                             return -1;

                                         return 0;

                                     })->on_result_ = [wr_this, _on_result](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == 0)
        {
            _on_result();
        }
    };
}

void im::save_auth_to_export(std::function<void()> _on_result)
{
    const std::wstring export_file = get_auth_parameters_filename_exported();

    save_auth_params_to_file(auth_params_, export_file, _on_result);
}

void im::get_mask_id_list(int64_t _seq)
{
    if (!masks_)
        create_masks(weak_from_this());

    masks_->get_mask_id_list(_seq);
}

void im::get_mask_preview(int64_t _seq, const std::string& mask_id)
{
    assert(masks_ && "call get_mask_id_list first");

    if (masks_)
        masks_->get_mask_preview(_seq, mask_id);
}

void im::get_mask_model(int64_t _seq)
{
    assert(masks_ && "call get_mask_id_list first");

    if (masks_)
        masks_->get_mask_model(_seq);
}

void im::get_mask(int64_t _seq, const std::string& mask_id)
{
    assert(masks_ && "call get_mask_id_list first");

    if (masks_)
        masks_->get_mask(_seq, mask_id);
}

void im::get_existent_masks(int64_t _seq)
{
    assert(masks_ && "call get_mask_id_list first");

    if (masks_)
        masks_->get_existent_masks(_seq);
}

void im::load_auth_and_fetch_parameters()
{
    const std::wstring file_name = get_auth_parameters_filename();
    const std::wstring file_name_exported = get_auth_parameters_filename_exported();
    const std::wstring file_name_fetch = get_fetch_parameters_filename();

    auto auth_params = std::make_shared<auth_parameters>();
    auto fetch_params = std::make_shared<fetch_parameters>();

    auto wr_this = weak_from_this();

    bool init_result = true;

    do
    {
        core::tools::binary_stream bs_auth;
        bool auth_params_loaded = bs_auth.load_from_file(file_name);

        if (g_core->get_local_pin_enabled())
            auth_params_loaded &= bs_auth.decrypt_aes256(local_pin_hash_);

        if (auth_params_loaded && auth_params->unserialize(bs_auth))
            break;

        core::tools::binary_stream bstream_exported;
        if (bstream_exported.load_from_file(file_name_exported))
        {
            bstream_exported.write<char>('\0');

            rapidjson::Document doc;
            if (!doc.Parse(bstream_exported.read_available()).HasParseError())
            {
                if (auth_params->unserialize(doc))
                    break;
            }
        }

        init_result = false;
    }
    while (false);

    if (init_result && auth_params->is_valid())
    {
        core::tools::binary_stream bs_fetch;
        if (bs_fetch.load_from_file(file_name_fetch) && fetch_params->unserialize(bs_fetch, auth_params->aimsid_))
        {
        }

        auth_params_ = auth_params;
        fetch_params_ = fetch_params;

        async_tasks_->run_async_function([]
        {
            return 0;

        })->on_result_ = [wr_this, auth_params, fetch_params](int32_t _error)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            ptr_this->load_cached_objects();
        };
    }
    else
    {
        g_core->unlogin(false);
    }
}


void im::store_fetch_parameters()
{
    auto bstream = std::make_shared<core::tools::binary_stream>();
    fetch_params_->serialize(*bstream);

    const std::wstring file_name = get_fetch_parameters_filename();

    async_tasks_->run_async_function([bstream, file_name]
    {
        if (!bstream->save_2_file(file_name))
            return -1;

        return 0;
    });
}

std::shared_ptr<async_task_handlers> im::load_active_dialogs()
{
    auto handler = std::make_shared<async_task_handlers>();
    auto wr_this = weak_from_this();

    std::wstring active_dialogs_file = get_active_dilaogs_file_name();
    auto active_dlgs = std::make_shared<active_dialogs>();

    async_tasks_->run_async_function([active_dialogs_file = std::move(active_dialogs_file), active_dlgs]
    {
        core::tools::binary_stream bstream;
        if (!bstream.load_from_file(active_dialogs_file))
            return -1;

        bstream.write<char>('\0');

        rapidjson::Document doc;
        if (doc.Parse((const char*) bstream.read(bstream.available())).HasParseError())
            return -1;

        return active_dlgs->unserialize(doc);

    })->on_result_ = [wr_this, active_dlgs, handler](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == 0)
        {
            ptr_this->active_dialogs_ = active_dlgs;
            //    ptr_this->post_active_dialogs_to_gui();
        }

        if (!active_dlgs->size())
            ptr_this->post_active_dialogs_are_empty_to_gui();

        if (handler->on_result_)
            handler->on_result_(_error);
    };

    return handler;
}

std::shared_ptr<async_task_handlers> im::load_contact_list()
{
    auto handler = std::make_shared<async_task_handlers>();
    auto wr_this = weak_from_this();

    const std::wstring contact_list_file = get_contactlist_file_name();
    auto contact_list = std::make_shared<contactlist>();

    async_tasks_->run_async_function([contact_list_file, contact_list]
    {
        core::tools::binary_stream bstream;
        if (!bstream.load_from_file(contact_list_file))
            return -1;

        bstream.write<char>('\0');

        rapidjson::Document doc;
        if (doc.Parse((const char*) bstream.read(bstream.available())).HasParseError())
            return -1;

        return contact_list->unserialize(doc);

    })->on_result_ = [wr_this, contact_list, handler](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == 0)
        {
            ptr_this->contact_list_ = contact_list;

            ptr_this->insert_friendly(contact_list->get_persons(), core::friendly_source::local);

            ptr_this->post_contact_list_to_gui();
        }

        if (handler->on_result_)
            handler->on_result_(_error);
    };

    return handler;
}

std::shared_ptr<async_task_handlers> im::load_favorites()
{
    auto handler = std::make_shared<async_task_handlers>();

    const std::wstring favorites_file = get_favorites_file_name();
    auto fvrts = std::make_shared<favorites>();

    async_tasks_->run_async_function([favorites_file, fvrts]
    {
        core::tools::binary_stream bstream;
        if (!bstream.load_from_file(favorites_file))
            return -1;

        bstream.write<char>('\0');

        rapidjson::Document doc;
        if (doc.Parse((const char*) bstream.read(bstream.available())).HasParseError())
            return -1;

        return fvrts->unserialize(doc);

    })->on_result_ = [wr_this = weak_from_this(), fvrts, handler](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == 0)
        {
            ptr_this->favorites_ = fvrts;

            core::stats::event_props_type props;
            props.emplace_back("count", std::to_string(fvrts->size()));
            g_core->insert_event(core::stats::stats_event_names::favorites_load, props);

            for (const auto& fvrt : ptr_this->favorites_->contacts())
            {
                const auto& aimid = fvrt.get_aimid();
                if (!ptr_this->contact_list_->is_ignored(aimid))
                {
                    ptr_this->insert_friendly(aimid, fvrt.get_friendly().name_, fvrt.get_friendly().nick_, fvrt.get_friendly().official_, friendly_source::local, friendly_add_mode::insert);
                    ptr_this->post_dlg_state_to_gui(aimid, true, true, true);
                }
            }
        }

        if (handler->on_result_)
            handler->on_result_(_error);
    };

    return handler;

}

std::shared_ptr<async_task_handlers> im::load_mailboxes()
{
    auto handler = std::make_shared<async_task_handlers>();
    auto wr_this = weak_from_this();

    std::wstring mailboxes_file = get_mailboxes_file_name();
    auto mailboxes = std::make_shared<mailbox_storage>();

    async_tasks_->run_async_function([mailboxes_file = std::move(mailboxes_file), mailboxes]
    {
        return mailboxes->load(mailboxes_file);
    })->on_result_ = [wr_this, mailboxes, handler](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == 0)
        {
            ptr_this->mailbox_storage_ = mailboxes;
            coll_helper coll(g_core->create_collection(), true);
            ptr_this->mailbox_storage_->serialize(coll);
            coll.set_value_as_bool("init", true);
            g_core->post_message_to_gui("mailboxes/status", 0, coll.get());
        }

        if (handler->on_result_)
            handler->on_result_(_error);
    };

    return handler;
}

std::shared_ptr<async_task_handlers> im::load_my_info()
{
    auto handler = std::make_shared<async_task_handlers>();
    auto wr_this = weak_from_this();

    const auto file_name = get_my_info_file_name();

    async_tasks_->run_async_function([wr_this, file_name]
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return 1;

        return ptr_this->my_info_cache_->load(file_name);

    })->on_result_ = [wr_this, handler](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == 0)
        {
            if (const auto my_info = ptr_this->my_info_cache_->get_info())
                ptr_this->insert_friendly(my_info->get_aimid(), my_info->get_friendly(), my_info->get_nick(), my_info->official(), core::friendly_source::local);
            ptr_this->post_my_info_to_gui();
        }

        if (handler->on_result_)
            handler->on_result_(_error);
    };

    return handler;
}

void im::resume_download_stickers()
{
    if (get_stickers()->is_download_stickers_error())
    {
        get_stickers()->set_download_stickers_error(false);

        download_stickers(false);
    }

    if (get_stickers()->is_download_meta_error())
    {
        get_stickers()->set_download_meta_error(false);

        auto wr_this = weak_from_this();

        get_stickers()->get_md5()->on_result_ = [wr_this](const std::string& _md5)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            ptr_this->download_stickers_metafile(0, get_strickers_size(), _md5);
        };
    }
}

void im::resume_download_masks()
{
    if (!masks_)
        create_masks(weak_from_this());

    masks_->on_connection_restored();
}

void im::post_stickers_meta_to_gui(int64_t _seq, const std::string& _size)
{
    coll_helper _coll(g_core->create_collection(), true);

    auto wr_this = weak_from_this();

    get_stickers()->serialize_meta(_coll, _size)->on_result_ = [wr_this, _seq](coll_helper _coll)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        g_core->post_message_to_gui("stickers/meta/get/result", _seq, _coll.get());
    };

    post_stickers_suggests_2_gui();
}

void im::post_stickers_store_to_gui(int64_t _seq)
{
    coll_helper _coll(g_core->create_collection(), true);

    auto wr_this = weak_from_this();

    get_stickers()->serialize_store(_coll)->on_result_ = [wr_this, _seq](coll_helper _coll)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        g_core->post_message_to_gui("stickers/store/get/result", _seq, _coll.get());
    };
}

void im::post_stickers_search_result_to_gui(int64_t _seq)
{
    coll_helper _coll(g_core->create_collection(), true);

    auto wr_this = weak_from_this();

    get_stickers()->serialize_store(_coll)->on_result_ = [wr_this, _seq](coll_helper _coll)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        g_core->post_message_to_gui("stickers/store/search/result", _seq, _coll.get());
    };
}

void im::download_stickers_metadata(int64_t _seq, const std::string& _size, const bool _up_to_date)
{
    auto wr_this = weak_from_this();

    get_stickers()->get_next_meta_task()->on_result_ = [wr_this, _size, _seq, _up_to_date](bool _res, const stickers::download_task& _task)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (!_res)
        {
            if (!_up_to_date)
            {
                ptr_this->post_stickers_meta_to_gui(_seq, _size);
            }

            ptr_this->download_stickers();

            ptr_this->get_stickers()->set_download_meta_in_progress(false);

            if (ptr_this->get_stickers()->is_flag_meta_need_reload())
            {
                ptr_this->get_stickers()->set_flag_meta_need_reload(false);

                ptr_this->get_stickers()->get_md5()->on_result_ = [wr_this, _size](const std::string& _md5)
                {
                    auto ptr_this = wr_this.lock();
                    if (!ptr_this)
                        return;

                    ptr_this->download_stickers_metafile(0, _size, _md5);
                };
            }

            return;
        }

        ptr_this->get_async_loader().download_file(top_priority(), _task.get_source_url(), _task.get_source_url(), _task.get_dest_file(), ptr_this->make_wim_params(), file_info_handler_t(
            [wr_this, _seq, _size, _task](loader_errors _error, const file_info_data_t& _data)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                if (loader_errors::network_error == _error)
                {
                    ptr_this->get_stickers()->set_download_meta_error(true);
                    ptr_this->get_stickers()->set_download_meta_in_progress(false);

                    return;
                }

                ptr_this->get_stickers()->on_metadata_loaded(_task)->on_result_ = [wr_this, _size, _seq, _task](bool)
                {
                    auto ptr_this = wr_this.lock();
                    if (!ptr_this)
                        return;

                    ptr_this->download_stickers_metadata(_seq, _size, false);
                };
            }), 0, -1, true, _task.get_endpoint());
    };
}

void post_sticker_fail_2_gui(int64_t _seq, int32_t _set_id, int32_t _sticker_id, std::string_view _fs_id, const loader_errors _error)
{
    coll_helper coll(g_core->create_collection(), true);

    if (_set_id)
        coll.set_value_as_int("set_id", _set_id);
    coll.set_value_as_int("sticker_id", _sticker_id);
    coll.set_value_as_string("fs_id", _fs_id);
    coll.set_value_as_int("error", (int32_t) _error);


    g_core->post_message_to_gui("stickers/sticker/get/result", _seq, coll.get());
}


void post_sticker_2_gui(int64_t _seq, int32_t _set_id, int32_t _sticker_id, std::string_view _fs_id, core::sticker_size _size, tools::binary_stream& _data)
{
    assert(_size > sticker_size::min);
    assert(_size < sticker_size::max);

    coll_helper coll(g_core->create_collection(), true);

    if (_set_id >= 0)
        coll.set_value_as_int("set_id", _set_id);
    coll.set_value_as_int("sticker_id", _sticker_id);
    coll.set_value_as_string("fs_id", _fs_id);
    coll.set_value_as_int("error", 0);

    const auto write_data =
        [&coll](tools::binary_stream &_data, std::string_view id)
    {
        if (_data.available() == 0)
        {
            return;
        }

        ifptr<istream> sticker_data(coll->create_stream(), true);
        const auto data_size = _data.available();
        sticker_data->write((const uint8_t*)_data.read(data_size), data_size);

        coll.set_value_as_stream(id, sticker_data.get());
    };

    if (_size == sticker_size::small)
    {
        write_data(_data, "data/small");
    }
    else if (_size == sticker_size::medium)
    {
        write_data(_data, "data/medium");
    }
    else if (_size == sticker_size::large)
    {
        write_data(_data, "data/large");
    }
    else if (_size == sticker_size::xlarge)
    {
        write_data(_data, "data/xlarge");
    }
    else if (_size == sticker_size::xxlarge)
    {
        write_data(_data, "data/xxlarge");
    }

    g_core->post_message_to_gui("stickers/sticker/get/result", _seq, coll.get());
}

void post_set_icon_2_gui(int64_t _seq, int32_t _set_id, tools::binary_stream& _data)
{
    coll_helper coll(g_core->create_collection(), true);

    coll.set_value_as_int("set_id", _set_id);
    coll.set_value_as_int("error", 0);

    if (_data.available() == 0)
    {
        return;
    }

    ifptr<istream> icon_data(coll->create_stream(), true);

    const auto data_size = _data.available();

    icon_data->write((const uint8_t*)_data.read(data_size), data_size);

    coll.set_value_as_stream("icon", icon_data.get());

    g_core->post_message_to_gui("stickers/big_set_icon/get/result", _seq, coll.get());
}

void im::load_stickers_data(int64_t _seq, const std::string& _size, const bool _up_to_date)
{
    auto wr_this = weak_from_this();

    get_stickers()->make_download_tasks(_size)->on_result_ = [wr_this, _seq, _size, _up_to_date](const std::vector<std::string>& _fs_ids)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->download_stickers_metadata(_seq, _size, _up_to_date);
    };
}

void im::on_im_created()
{
    im_created_ = true;

    g_core->post_message_to_gui("im/created", 0, nullptr);

    schedule_store_timer();
    schedule_statistic_timer();
    schedule_ui_activity_timer();
}

void im::load_cached_objects()
{
    auto wr_this = weak_from_this();

    auto call_on_exit = std::make_shared<tools::auto_scope_bool>([wr_this](const bool _success)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_success)
        {
            ptr_this->poll(true, im::poll_reason::after_first_start);
        }
        else
        {
            if (ptr_this->auth_params_->is_valid_token())
            {
                ptr_this->start_session();
            }
            else if (ptr_this->auth_params_->is_valid_agent_token())
            {
                ptr_this->login_by_agent_token(
                    ptr_this->auth_params_->login_,
                    ptr_this->auth_params_->agent_token_,
                    ptr_this->auth_params_->product_guid_8x_);
            }
            else
            {
                ptr_this->login_by_password(
                    0,
                    ptr_this->auth_params_->login_,
                    ptr_this->auth_params_->password_md5_,
                    true,
                    true,
                    true);
            }

        }
    });

    set_connection_state(connection_state::connecting);

    tools::version_info info_this;

    const bool is_version_empty = auth_params_->version_.empty();

    bool is_new_version = true;

    if (!is_version_empty)
        is_new_version = (tools::version_info(auth_params_->version_) != info_this);

    const bool is_locale_changed = (g_core->get_locale() != auth_params_->locale_);

    if (is_version_empty || is_new_version || is_locale_changed)
        return;

    load_contact_list()->on_result_ = [wr_this, call_on_exit](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error != 0)
            return;

        ptr_this->load_mailboxes()->on_result_ = [wr_this, call_on_exit](int32_t /*_error*/)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            ptr_this->load_my_info()->on_result_ = [wr_this, call_on_exit](int32_t _error)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                if (_error != 0)
                    return;

                ptr_this->load_favorites()->on_result_ = [wr_this, call_on_exit](int32_t /*_error*/)
                {
                    auto ptr_this = wr_this.lock();
                    if (!ptr_this)
                        return;

                    ptr_this->load_active_dialogs()->on_result_ = [wr_this, call_on_exit](int32_t _error)
                    {
                        auto ptr_this = wr_this.lock();
                        if (!ptr_this)
                            return;

                        if (_error == 0)
                        {
                            call_on_exit->set_success();

                            auto avatar_size = g_core->get_core_gui_settings().recents_avatars_size_;

                            ptr_this->active_dialogs_->enumerate([wr_this, avatar_size](const active_dialog& _dlg)
                            {
                                auto ptr_this = wr_this.lock();
                                if (!ptr_this)
                                    return;

                                const auto& dlg_aimid = _dlg.get_aimid();

                                if (!ptr_this->contact_list_->is_ignored(dlg_aimid))
                                {
                                    if (avatar_size > 0)
                                        ptr_this->get_contact_avatar(initial_request_avatar_seq, _dlg.get_aimid(), avatar_size, false);

                                    ptr_this->post_dlg_state_to_gui(dlg_aimid, false, true, false, true);
                                }

                                ptr_this->get_archive()->get_gallery_state(dlg_aimid)->on_result = [wr_this, dlg_aimid](const archive::gallery_state& _state)
                                {
                                    auto ptr_this = wr_this.lock();
                                    if (!ptr_this)
                                        return;

                                    ptr_this->post_gallery_state_to_gui(dlg_aimid, _state);
                                };
                            });

                            ptr_this->post_ignorelist_to_gui(0);

                            g_core->post_message_to_gui("cachedobjects/loaded", 0, nullptr);
                        }
                    };
                };
            };
        };
    };
}

void im::download_stickers_metafile(int64_t _seq, const std::string& _size, const std::string& _md5)
{
    if (get_stickers()->is_download_meta_error())
        return;

    if (get_stickers()->is_download_meta_in_progress())
        return;

    get_stickers()->set_up_to_date(false);
    get_stickers()->set_download_meta_in_progress(true);
    get_stickers()->set_flag_meta_need_reload(false);

    auto packet = std::make_shared<get_stickers_index>(make_wim_params(), _md5);

    post_wim_packet(packet)->on_result_ = [wr_this = weak_from_this(), packet, _seq, _size](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == 0)
        {
            auto response = packet->get_response();

            ptr_this->get_stickers()->parse(response, false)->on_result_ = [response, wr_this, _seq, _size](const stickers::parse_result& _res)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                if (_res.is_success())
                {
                    ptr_this->get_stickers()->set_up_to_date(_res.is_up_to_date());

                    const bool up_to_date = _res.is_up_to_date();

                    if (!up_to_date)
                    {
                        response->reset_out();

                        ptr_this->get_stickers()->save(response)->on_result_ = [wr_this, _seq, _size, up_to_date](bool _res)
                        {
                            auto ptr_this = wr_this.lock();
                            if (!ptr_this)
                                return;

                            ptr_this->load_stickers_data(_seq, _size, up_to_date);
                        };
                    }
                    else
                    {
                        ptr_this->load_stickers_data(_seq, _size, up_to_date);
                    }
                }
            };
        }
        else
        {
            if (wim_packet::needs_to_repeat_failed(_error))
            {
                ptr_this->get_stickers()->set_download_meta_error(true);
                ptr_this->get_stickers()->set_download_meta_in_progress(false);
            }
        }
    };
}

void im::get_stickers_meta(int64_t _seq, const std::string& _size)
{
    set_stickers_size(_size);

    get_stickers()->load_meta_from_local()->on_result_ = [wr_this = weak_from_this(), stickers_size = _size, _seq](const stickers::load_result& _res)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->get_stickers()->set_gui_request_params(stickers::gui_request_params(stickers_size, _seq, _res.get_md5()));

        if (_res.get_result())
            ptr_this->post_stickers_meta_to_gui(_seq, stickers_size);

        ptr_this->download_stickers_metafile(_seq, stickers_size, _res.get_md5());
    };
}

void im::reload_stickers_meta(const int64_t _seq, const std::string& _size)
{
    get_stickers()->get_md5()->on_result_ = [wr_this = weak_from_this(), _size](const std::string& _md5)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (ptr_this->get_stickers()->is_download_meta_in_progress())
        {
            ptr_this->get_stickers()->set_flag_meta_need_reload(true);
        }
        else
        {
            ptr_this->download_stickers_metafile(0, _size, _md5);
        }
    };
}

void im::get_stickers_pack_info(const int64_t _seq, const int32_t _set_id, const std::string& _store_id, const std::string& _file_id)
{
    auto packet = std::make_shared<get_stickers_pack_info_packet>(make_wim_params(), _set_id, _store_id, _file_id);

    post_wim_packet(packet)->on_result_ = [wr_this = weak_from_this(), packet, _seq, _set_id, _store_id, _file_id](int32_t _error)
    {
        coll_helper coll_result(g_core->create_collection(), true);

        coll_result.set_value_as_bool("result", false);
        coll_result.set_value_as_int("set_id", _set_id);
        coll_result.set_value_as_string("store_id", _store_id);
        coll_result.set_value_as_string("file_id", _file_id);
        coll_result.set_value_as_int("http_code", packet->get_http_code());

        const auto auto_handler = std::make_shared<tools::auto_scope>([coll_result, _seq]()
        {
            g_core->post_message_to_gui("stickers/pack/info/result", _seq, coll_result.get());

        });

        if (_error == 0)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            auto response = packet->get_response();
            if (!response)
                return;

            rapidjson::Document doc;
            const auto& parse_result = doc.ParseInsitu(response->read(response->available()));
            if (parse_result.HasParseError())
                return;

            auto status_code = 0;
            if (!tools::unserialize_value(doc, "status", status_code))
                return;

            if (status_code != 200)
            {
                assert(false);
                return;
            }

            auto iter_data = doc.FindMember("data");
            if (iter_data == doc.MemberEnd() || !iter_data->value.IsObject())
                return;

            ptr_this->get_stickers()->update_template_urls(iter_data->value);

            stickers::set set;
            if (!set.unserialize(iter_data->value))
                return;

            coll_helper coll_set(g_core->create_collection(), true);

            stickers::cache::serialize_meta_set_sync(set, coll_set, get_strickers_size());

            coll_result.set_value_as_bool("result", true);

            coll_result.set_value_as_collection("info", coll_set.get());
        }
        else
        {

        }
    };
}

void im::add_stickers_pack(const int64_t _seq, const int32_t _set_id, std::string _store_id)
{
    auto packet = std::make_shared<add_stickers_pack_packet>(make_wim_params(), _set_id, std::move(_store_id));

    post_wim_packet(packet)->on_result_ = [_seq](int32_t _error)
    {
        coll_helper coll(g_core->create_collection(), true);

        coll.set_value_as_int("error", _error);

        g_core->post_message_to_gui("stickers/pack/add/result", _seq, coll.get());
    };
}

void im::remove_stickers_pack(const int64_t _seq, const int32_t _set_id, std::string _store_id)
{
    auto packet = std::make_shared<remove_stickers_pack_packet>(make_wim_params(), _set_id, std::move(_store_id));

    post_wim_packet(packet)->on_result_ = [_seq](int32_t _error)
    {
        coll_helper coll(g_core->create_collection(), true);

        coll.set_value_as_int("error", _error);

        g_core->post_message_to_gui("stickers/pack/remove/result", _seq, coll.get());
    };
}

void im::get_stickers_store(const int64_t _seq)
{
    auto packet = std::make_shared<get_stickers_store_packet>(make_wim_params());

    post_wim_packet(packet)->on_result_ = [wr_this = weak_from_this(), packet, _seq](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == 0)
        {
            auto response = packet->get_response();

            ptr_this->get_stickers()->parse_store(response)->on_result_ = [response, wr_this, _seq](const bool _result)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                ptr_this->post_stickers_store_to_gui(_seq);
            };
        }
    };
}

void im::search_stickers_store(const int64_t _seq, const std::string& _search_term)
{
    auto wr_this = weak_from_this();

    auto packet = std::make_shared<get_stickers_store_packet>(make_wim_params(), _search_term);

    post_wim_packet(packet)->on_result_ = [wr_this, packet, _seq](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == 0)
        {
            auto response = packet->get_response();

            ptr_this->get_stickers()->parse_store(response)->on_result_ = [wr_this, _seq](const bool _result)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                ptr_this->post_stickers_search_result_to_gui(_seq);
            };
        }
    };
}

void im::set_sticker_order(const int64_t _seq, const std::vector<int32_t>& _values)
{
    auto packet = std::make_shared<set_stickers_order_packet>(make_wim_params(), _values);

    post_wim_packet(packet)->on_result_ = [wr_this = weak_from_this(), packet, _seq](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        coll_helper coll(g_core->create_collection(), true);

        coll.set_value_as_int("error", _error);

        g_core->post_message_to_gui("stickers/set/order/result", _seq, coll.get());
    };
}

void im::get_fs_stickers_by_ids(const int64_t _seq, std::vector<std::pair<int32_t, int32_t>> _values)
{
    auto packet = std::make_shared<stickers_migration>(make_wim_params(), _values);

    post_wim_packet(packet)->on_result_ = [wr_this = weak_from_this(), packet, _seq, _values = std::move(_values)](int32_t _error) mutable
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (wim_packet::needs_to_repeat_failed(_error))
        {
            ptr_this->failed_sticker_migration_ = std::move(_values);
        }
        else
        {
            coll_helper coll(g_core->create_collection(), true);
            ifptr<iarray> stickers_array(coll->create_array());

            const auto auto_handler = std::make_shared<tools::auto_scope>([coll, stickers_array, _seq]()
            {
                g_core->post_message_to_gui("stickers/fs_by_ids/get/result", _seq, coll.get());
            });

            auto response = packet->get_response();
            if (!response)
                return;

            rapidjson::Document doc;
            const auto& parse_result = doc.ParseInsitu(response->read(response->available()));
            if (parse_result.HasParseError())
                return;

            auto status_code = 0;
            if (!tools::unserialize_value(doc, "status", status_code))
                return;

            if (status_code != 200)
            {
                assert(false);
                return;
            }

            const auto iter_data = doc.FindMember("data");
            if (iter_data == doc.MemberEnd() || !iter_data->value.IsObject())
                return;

            for (auto sticker_it = iter_data->value.MemberBegin(); sticker_it != iter_data->value.MemberEnd(); ++sticker_it)
            {
                if (!sticker_it->value.IsString())
                    continue;

                const auto ids = rapidjson_get_string_view(sticker_it->name);
                const auto delim_pos = ids.find(':');

                if (delim_pos == ids.npos)
                {
                    assert(false);
                    continue;
                }

                try
                {
                    const auto pack_id = std::stoi(std::string(ids.substr(0, delim_pos))); // TODO use std::from_chars
                    const auto sticker_id = std::stoi(std::string(ids.substr(delim_pos + 1))); // TODO use std::from_chars
                    const auto fs_id = rapidjson_get_string_view(sticker_it->value);

                    coll_helper coll_message(coll->create_collection(), true);
                    coll_message.set_value_as_int("set_id", pack_id);
                    coll_message.set_value_as_int("id", sticker_id);
                    coll_message.set_value_as_string("fs_id", fs_id);
                    ifptr<ivalue> val(coll->create_value());
                    val->set_as_collection(coll_message.get());
                    stickers_array->push_back(val.get());
                }
                catch (...)
                {
                    assert(false);
                    continue;
                }
            }
            coll.set_value_as_array("stickers", stickers_array.get());
        }
    };
}

void im::get_set_icon_big(const int64_t _seq, const int32_t _set_id)
{
    auto wr_this = weak_from_this();

    get_stickers()->get_set_icon_big(_seq, _set_id)->on_result_ =
        [_seq, _set_id, wr_this](tools::binary_stream& _icon_data)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_icon_data.available())
        {
            post_set_icon_2_gui(_seq, _set_id, _icon_data);

            return;
        }

        ptr_this->download_stickers();
    };
}

void im::clean_set_icon_big(const int64_t _seq, const int32_t _set_id)
{
    get_stickers()->clean_set_icon_big(_seq, _set_id);
}


void im::download_stickers(const bool _recursive)
{
    if (get_stickers()->is_download_stickers_error())
        return;

    if (get_stickers()->is_download_stickers_in_progress() && !_recursive)
        return;

    auto wr_this = weak_from_this();

    get_stickers()->set_download_stickers_in_progress(true);

    get_stickers()->get_next_sticker_task()->on_result_ = [wr_this](bool _res, const stickers::download_task& _task)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (!_res)
        {
            ptr_this->get_stickers()->set_download_stickers_in_progress(false);
            return;
        }

        //ATLTRACE("download sticker %s\r\n", _task.get_source_url().c_str());

        ptr_this->get_async_loader().download_file(high_priority(), _task.get_source_url(), _task.get_source_url(), _task.get_dest_file(), ptr_this->make_wim_params(),
            file_info_handler_t([wr_this, _task](loader_errors _error, const file_info_data_t& _data)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            // if network error
            if (_error == loader_errors::network_error)
            {
                ptr_this->get_stickers()->set_download_stickers_error(true);
                ptr_this->get_stickers()->set_download_stickers_in_progress(false);

                return;
            }

            int32_t set_id = _task.get_set_id();
            int32_t sticker_id = _task.get_sticker_id();
            sticker_size sz = _task.get_size();
            auto fs_id = _task.get_fs_id();

            ptr_this->get_stickers()->on_sticker_loaded(_task)->on_result_ = [wr_this, set_id, sticker_id, sz, _error, fs_id = std::move(fs_id)](bool _res, const std::list<int64_t>& _requests)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                if (_res && !_requests.empty())
                {
                    if (_error == loader_errors::success)
                    {
                        // big icon
                        if (sticker_id == -1 && fs_id.empty())
                        {
                            ptr_this->get_stickers()->get_set_icon_big(0, set_id)->on_result_ = [_requests, set_id](tools::binary_stream& _data)
                            {
                                for (auto seq : _requests)
                                    post_set_icon_2_gui(seq, set_id, _data);
                            };
                        }
                        else
                        {
                            ptr_this->get_stickers()->get_sticker(
                                0,
                                set_id,
                                sticker_id,
                                fs_id,
                                sz)->on_result_ = [_requests, set_id, sticker_id, fs_id, sz](tools::binary_stream& _data)
                            {
                                for (auto seq : _requests)
                                    post_sticker_2_gui(seq, set_id, sticker_id, fs_id, sz, _data);
                            };
                        }
                    }
                    else
                    {
                        for (auto seq : _requests)
                            post_sticker_fail_2_gui(seq, set_id, sticker_id, fs_id, _error);
                    }
                }

                ptr_this->download_stickers(true);
            };
        }), 0, -1, true, _task.get_endpoint());
    };
}


void im::get_sticker(const int64_t _seq, const int32_t _set_id, const int32_t _sticker_id, std::string_view _fs_id, const core::sticker_size _size)
{
    assert(_size > sticker_size::min);
    assert(_size < sticker_size::max);

    get_stickers()->get_sticker(_seq, _set_id, _sticker_id, std::string(_fs_id), _size)->on_result_ =
        [_seq, _set_id, _sticker_id, fs_id = std::string(_fs_id), _size, wr_this = weak_from_this()](tools::binary_stream& _sticker_data)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_sticker_data.available())
        {
            post_sticker_2_gui(_seq, _set_id, _sticker_id, fs_id, _size, _sticker_data);

            return;
        }

        ptr_this->download_stickers();
    };
}

void im::post_stickers_suggests_2_gui()
{
    coll_helper coll(g_core->create_collection(), true);

    get_stickers()->serialize_suggests(coll)->on_result_ = [coll](const bool _res)
    {
        g_core->post_message_to_gui("stickers/suggests", 0, coll.get());
    };
}

void im::get_chat_info(int64_t _seq, const std::string& _aimid, const std::string& _stamp, int32_t _limit)
{
    get_chat_info_params params;
    params.aimid_ = _aimid;
    params.stamp_ = _stamp;
    params.members_limit_ = _limit;

    auto packet = std::make_shared<core::wim::get_chat_info>(make_wim_params(), std::move(params));

    post_robusto_packet(packet)->on_result_ = [wr_this = weak_from_this(), _seq, packet, _limit, _aimid](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == 0)
        {
            ptr_this->insert_friendly(packet->get_persons(), friendly_source::remote);
            coll_helper coll(g_core->create_collection(), true);
            packet->get_result().serialize(coll);
            coll.set_value_as_int("members_limit", _limit);

            g_core->post_message_to_gui("chats/info/get/result", _seq, coll.get());
        }
        else
        {
            coll_helper coll(g_core->create_collection(), true);

            int32_t error_code = -1;
            switch (_error)
            {
            case wpie_error_robusto_you_are_not_chat_member:
                error_code = (int32_t)core::group_chat_info_errors::not_in_chat;
                break;
            case wpie_network_error:
                error_code = (int32_t)core::group_chat_info_errors::network_error;
                break;
            case  wpie_error_robusto_you_are_blocked:
                error_code = (int32_t)core::group_chat_info_errors::blocked;
                break;
            default:
                error_code = (int32_t)core::group_chat_info_errors::min;
            }

            coll.set_value_as_int("error", error_code);
            coll.set_value_as_string("aimid", _aimid);
            g_core->post_message_to_gui("chats/info/get/failed", _seq, coll.get());
        }
    };
}

void im::resolve_pending(int64_t _seq, const std::string& _aimid, const std::vector<std::string>& _contacts, bool _approve)
{
    auto packet = std::make_shared<core::wim::resolve_pending>(make_wim_params(), _aimid, _contacts, _approve);
    post_robusto_packet(packet)->on_result_ = [_seq, packet](int32_t _error)
    {
        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_int("error", _error);
        g_core->post_message_to_gui("chats/pending/resolve/result", _seq, coll.get());
    };
}

void core::wim::im::get_chat_member_info(int64_t _seq, const std::string & _aimid, const std::vector<std::string>& _members)
{
    auto packet = std::make_shared<core::wim::get_chat_member_info>(make_wim_params(), _aimid, _members);

    auto wr_this = weak_from_this();
    post_robusto_packet(packet)->on_result_ = [wr_this, _seq, packet](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        coll_helper coll(g_core->create_collection(), true);

        if (_error == 0)
            packet->get_result().serialize(coll);
        else
            coll.set_value_as_int("error", _error);

        g_core->post_message_to_gui("chats/member/info/result", _seq, coll.get());
    };
}

void im::get_chat_members_page(int64_t _seq, std::string_view _aimid, std::string_view _role, uint32_t _page_size)
{
    auto packet = std::make_shared<core::wim::get_chat_members>(make_wim_params(), _aimid, _role, _page_size);

    auto wr_this = weak_from_this();
    post_robusto_packet(packet)->on_result_ = [wr_this, _seq, packet, _page_size](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->on_get_chat_members_result(_seq, packet, _page_size, _error);
    };
}

void im::get_chat_members_next_page(int64_t _seq, std::string_view _aimid, std::string_view _cursor, uint32_t _page_size)
{
    auto packet = std::make_shared<core::wim::get_chat_members>(make_wim_params(), _aimid, std::string_view(), _page_size, _cursor);

    auto wr_this = weak_from_this();
    post_robusto_packet(packet)->on_result_ = [wr_this, _seq, packet, _page_size](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->on_get_chat_members_result(_seq, packet, _page_size, _error);
    };
}

void im::on_get_chat_members_result(int64_t _seq, const std::shared_ptr<get_chat_members>& _packet, uint32_t _page_size, int32_t _error)
{
    coll_helper coll(g_core->create_collection(), true);

    if (_error == 0)
    {
        insert_friendly(_packet->get_persons(), friendly_source::remote);

        _packet->get_result().serialize(coll);
        coll.set_value_as_int("page_size", _page_size);
        if (_packet->is_reset_pages())
            coll.set_value_as_bool("reset_pages", true);
    }
    else
    {
        coll.set_value_as_int("error", _error);
    }

    g_core->post_message_to_gui("chats/members/get/result", _seq, coll.get());
}

void core::wim::im::search_chat_members_page(int64_t _seq, std::string_view _aimid, std::string_view _role, std::string_view _keyword, uint32_t _page_size)
{
    auto packet = std::make_shared<core::wim::search_chat_members>(make_wim_params(), _aimid, _role, _keyword, _page_size);

    auto wr_this = weak_from_this();
    post_robusto_packet(packet)->on_result_ = [wr_this, _seq, packet, _page_size](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->on_search_chat_members_result(_seq, packet, _page_size, _error);
    };
}

void core::wim::im::search_chat_members_next_page(int64_t _seq, std::string_view _aimid, std::string_view _cursor, uint32_t _page_size)
{
    auto packet = std::make_shared<core::wim::search_chat_members>(make_wim_params(), _aimid, std::string_view(), std::string_view(), _page_size, _cursor);

    auto wr_this = weak_from_this();
    post_robusto_packet(packet)->on_result_ = [wr_this, _seq, packet, _page_size](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->on_search_chat_members_result(_seq, packet, _page_size, _error);
    };
}

void core::wim::im::on_search_chat_members_result(int64_t _seq, const std::shared_ptr<search_chat_members>& _packet, uint32_t _page_size, int32_t _error)
{
    coll_helper coll(g_core->create_collection(), true);

    if (_error == 0)
    {
        insert_friendly(_packet->get_persons(), friendly_source::remote);

        _packet->get_result().serialize(coll);
        coll.set_value_as_int("page_size", _page_size);
        if (_packet->is_reset_pages())
            coll.set_value_as_bool("reset_pages", true);
    }
    else
    {
        coll.set_value_as_int("error", _error);
    }

    g_core->post_message_to_gui("chats/members/search/result", _seq, coll.get());
}

void im::get_chat_contacts(int64_t _seq, const std::string& _aimid, const std::string& _cursor, uint32_t _page_size)
{
    auto wr_this = weak_from_this();

    auto packet = std::make_shared<core::wim::get_chat_contacts>(make_wim_params(), _aimid, _cursor, _page_size);

    post_robusto_packet(packet)->on_result_ = [wr_this, _seq, packet, _aimid](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == 0)
        {
            coll_helper coll(g_core->create_collection(), true);
            packet->get_result().serialize(coll);
            coll.set_value_as_string("aimid", _aimid);
            g_core->post_message_to_gui("chats/contacts/result", _seq, coll.get());
        }
    };
}

void im::create_chat(int64_t _seq, const std::string& _aimid, const std::string& _name, std::vector<std::string> _members, core::wim::chat_params _params)
{
    auto packet = std::make_shared<core::wim::create_chat>(make_wim_params(), _aimid, _name, std::move(_members));
    packet->set_chat_params(std::move(_params));
    post_robusto_packet(packet)->on_result_ = [_seq, packet](int32_t _error)
    {
        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_int("error", _error);
        g_core->post_message_to_gui("chats/create/result", _seq, coll.get());

        if (_error == 0)
        {
            core::stats::event_props_type props;
            auto members_count = packet->members_count();
            props.emplace_back(std::make_pair("Groupchat_Create_MembersCount", std::to_string(members_count)));
            g_core->insert_event(stats::stats_event_names::chats_created, props);
        }
    };
}

void im::mod_chat_params(int64_t _seq, const std::string& _aimid, core::wim::chat_params _params)
{
    auto packet = std::make_shared<core::wim::mod_chat_alpha>(make_wim_params(), _aimid);
    packet->set_chat_params(std::move(_params));
    post_robusto_packet(packet)->on_result_ = [_seq, packet](int32_t _error)
    {
        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_int("error", _error);
        g_core->post_message_to_gui("chats/mod/params/result", _seq, coll.get());
    };
}

void im::mod_chat_name(int64_t _seq, const std::string& _aimid, const std::string& _name)
{
    auto packet = std::make_shared<core::wim::mod_chat_alpha>(make_wim_params(), _aimid);
    packet->get_chat_params().set_name(_name);
    post_robusto_packet(packet)->on_result_ = [_seq, packet](int32_t _error)
    {
        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_int("error", _error);
        g_core->post_message_to_gui("chats/mod/name/result", _seq, coll.get());
    };
}

void im::mod_chat_about(int64_t _seq, const std::string& _aimid, const std::string& _about)
{
    auto packet = std::make_shared<core::wim::mod_chat_alpha>(make_wim_params(), _aimid);
    packet->get_chat_params().set_about(_about);
    post_robusto_packet(packet)->on_result_ = [_seq, packet](int32_t _error)
    {
        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_int("error", _error);
        g_core->post_message_to_gui("chats/mod/about/result", _seq, coll.get());
    };
}

void im::mod_chat_rules(int64_t _seq, const std::string& _aimid, const std::string& _rules)
{
    auto packet = std::make_shared<core::wim::mod_chat_alpha>(make_wim_params(), _aimid);
    packet->get_chat_params().set_rules(_rules);
    post_robusto_packet(packet)->on_result_ = [_seq, packet](int32_t _error)
    {
        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_int("error", _error);
        g_core->post_message_to_gui("chats/mod/about/result", _seq, coll.get());
    };
}

void im::mod_chat_public(int64_t _seq, const std::string& _aimid, bool _public)
{
    auto packet = std::make_shared<core::wim::mod_chat_alpha>(make_wim_params(), _aimid);
    packet->get_chat_params().set_public(_public);
    post_robusto_packet(packet)->on_result_ = [_seq, packet](int32_t _error)
    {
        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_int("error", _error);
        g_core->post_message_to_gui("chats/mod/public/result", _seq, coll.get());
    };
}

void im::mod_chat_join(int64_t _seq, const std::string& _aimid, bool _approved)
{
    auto packet = std::make_shared<core::wim::mod_chat_alpha>(make_wim_params(), _aimid);
    packet->get_chat_params().set_join(_approved);
    post_robusto_packet(packet)->on_result_ = [_seq, packet](int32_t _error)
    {
        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_int("error", _error);
        g_core->post_message_to_gui("chats/mod/public/result", _seq, coll.get());
    };
}

void im::mod_chat_link(int64_t _seq, const std::string& _aimid, bool _link)
{
    auto packet = std::make_shared<core::wim::mod_chat_alpha>(make_wim_params(), _aimid);
    packet->get_chat_params().set_joiningByLink(_link);
    post_robusto_packet(packet)->on_result_ = [_seq, packet](int32_t _error)
    {
        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_int("error", _error);
        g_core->post_message_to_gui("chats/mod/link/result", _seq, coll.get());
    };
}

void im::mod_chat_ro(int64_t _seq, const std::string& _aimid, bool _ro)
{
    auto packet = std::make_shared<core::wim::mod_chat_alpha>(make_wim_params(), _aimid);
    packet->get_chat_params().set_readOnly(_ro);
    post_robusto_packet(packet)->on_result_ = [_seq, packet](int32_t _error)
    {
        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_int("error", _error);
        g_core->post_message_to_gui("chats/mod/ro/result", _seq, coll.get());
    };
}

void im::set_attention_attribute(int64_t _seq, std::string _aimid, const bool _value, const bool _resume)
{
    if (!_resume)
        get_archive()->update_attention_attribute(_aimid, _value);

    auto wr_this = weak_from_this();

    auto handler = post_wim_packet(std::make_shared<core::wim::set_attention_attribute>(make_wim_params(), _aimid, _value));

    handler->on_result_ = [_aimid, wr_this](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (wim_packet::needs_to_repeat_failed(_error))
        {
            ptr_this->failed_set_attention_.insert(_aimid);
        }
    };
}

void core::wim::im::pin_chat_message(int64_t _seq, const std::string & _aimid, int64_t _message, bool _is_unpin)
{
    auto packet = std::make_shared<core::wim::pin_message>(make_wim_params(), _aimid, _message, _is_unpin);
    post_robusto_packet(packet);
}

void core::wim::im::get_mentions_suggests(int64_t _seq, const std::string& _aimid, const std::string& _pattern)
{
    auto wr_this = weak_from_this();
    auto packet = std::make_shared<core::wim::get_mentions_suggests>(make_wim_params(), _aimid, _pattern);
    post_robusto_packet(packet)->on_result_ = [wr_this, _seq, packet](int32_t _error)
    {
        if (_error == 0)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            ptr_this->insert_friendly(packet->get_persons(), friendly_source::remote);
            coll_helper coll(g_core->create_collection(), true);

            const auto& suggests = packet->get_suggests();
            if (!suggests.empty())
            {
                ifptr<iarray> arr(coll->create_array());
                arr->reserve(suggests.size());

                for (const auto& p : suggests)
                {
                    coll_helper coll_ment(coll->create_collection(), true);

                    coll_ment.set_value_as_string("aimid", p.aimid_);
                    coll_ment.set_value_as_string("friendly", p.friendly_);
                    if (!p.nick_.empty())
                        coll_ment.set_value_as_string("nick", p.nick_);

                    ifptr<ivalue> val(coll->create_value());
                    val->set_as_collection(coll_ment.get());
                    arr->push_back(val.get());
                }

                coll.set_value_as_array("suggests", arr.get());
            }

            g_core->post_message_to_gui("mentions/suggests/result", _seq, coll.get());
        }
    };
}

void im::block_chat_member(int64_t _seq, const std::string& _aimid, const std::string& _contact, bool _block)
{
    auto packet = std::make_shared<core::wim::block_chat_member>(make_wim_params(), _aimid, _contact, _block);
    post_robusto_packet(packet)->on_result_ = [_seq, packet](int32_t _error)
    {
        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_int("error", _error);
        g_core->post_message_to_gui("chats/block/result", _seq, coll.get());
    };
}

void im::set_chat_member_role(int64_t _seq, const std::string& _aimid, const std::string& _contact, const std::string& _role)
{
    auto packet = std::make_shared<core::wim::mod_chat_member_alpha>(make_wim_params(), _aimid, _contact, _role);
    post_robusto_packet(packet)->on_result_ = [_seq, packet](int32_t _error)
    {
        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_int("error", _error);
        g_core->post_message_to_gui("chats/role/set/result", _seq, coll.get());
    };
}

void im::get_chat_home(int64_t _seq, const std::string& _tag)
{
    auto wr_this = weak_from_this();

    auto packet = std::make_shared<core::wim::get_chat_home>(make_wim_params(), _tag);

    post_robusto_packet(packet)->on_result_ = [wr_this, _seq, packet](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == 0)
        {
            coll_helper coll(g_core->create_collection(), true);
            coll.set_value_as_string("new_tag", packet->result_tag_);
            coll.set_value_as_bool("need_restart", packet->need_restart_);
            coll.set_value_as_bool("finished", packet->finished_);
            ifptr<iarray> chats_array(coll->create_array());
            if (!packet->result_.empty())
            {
                chats_array->reserve((int32_t)packet->result_.size());
                for (const auto& chat : packet->result_)
                {
                    coll_helper coll_message(coll->create_collection(), true);
                    chat.serialize(coll_message);
                    ifptr<ivalue> val(coll->create_value());
                    val->set_as_collection(coll_message.get());
                    chats_array->push_back(val.get());
                }
            }
            coll.set_value_as_array("chats", chats_array.get());
            g_core->post_message_to_gui("chats/home/get/result", _seq, coll.get());
        }
        else
        {
            coll_helper coll(g_core->create_collection(), true);
            coll.set_value_as_int("error", _error);
            g_core->post_message_to_gui("chats/home/get/failed", _seq, coll.get());
        }
    };
}

void im::save_my_info()
{
    if (my_info_cache_ && my_info_cache_->is_changed())
        my_info_cache_->save(get_my_info_file_name());
}

void im::save_contact_list(force_save _force)
{
    static int counter = 0;
    const auto changed_status = contact_list_->get_changed_status();
    if (changed_status == core::wim::contactlist::changed_status::none)
        return;

    if (changed_status == core::wim::contactlist::changed_status::presence && _force != force_save::yes)
    {
        constexpr int presence_interval = 6 * 10;
        if (++counter < presence_interval)
            return;
    }
    counter = 0;

    std::wstring contact_list_file = get_contactlist_file_name();

    rapidjson::Document doc(rapidjson::Type::kObjectType);
    contact_list_->serialize(doc, doc.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::string json_string = rapidjson_get_string(buffer);

    if (!json_string.length())
    {
        assert( false );
        return;
    }

    contact_list_->set_changed_status(core::wim::contactlist::changed_status::none);

    auto handler = async_tasks_->run_async_function([contact_list_file, str = std::move(json_string)]
    {
        core::tools::binary_stream bstream;
        bstream.write<std::string>(str);
        if (!bstream.save_2_file(contact_list_file))
            return -1;

        return 0;
    });
}

void im::save_active_dialogs()
{
    if (!active_dialogs_->is_changed())
        return;

    std::wstring active_dialogs_file = get_active_dilaogs_file_name();

    rapidjson::Document doc(rapidjson::Type::kObjectType);
    active_dialogs_->serialize(doc, doc.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::string json_string = rapidjson_get_string(buffer);

    if (!json_string.length())
    {
        assert( false );
        return;
    }

    active_dialogs_->set_changed(false);

    auto handler = async_tasks_->run_async_function([active_dialogs_file, str = std::move(json_string)]
    {
        core::tools::binary_stream bstream;
        bstream.write<std::string>(str);
        if (!bstream.save_2_file(active_dialogs_file))
            return -1;

        return 0;
    });
}

void im::save_favorites()
{
    if (!favorites_->is_changed())
        return;

    std::wstring favorites_file = get_favorites_file_name();

    rapidjson::Document doc(rapidjson::Type::kObjectType);
    favorites_->serialize(doc, doc.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::string json_string = rapidjson_get_string(buffer);

    if (!json_string.length())
    {
        assert( false );
        return;
    }

    favorites_->set_changed(false);

    auto handler = async_tasks_->run_async_function([favorites_file = std::move(favorites_file), str = std::move(json_string)]
    {
        core::tools::binary_stream bstream;
        bstream.write<std::string>(str);
        if (!bstream.save_2_file(favorites_file))
            return -1;

        return 0;
    });
}

void im::save_mailboxes()
{
    if (!mailbox_storage_)
        return;

    mailbox_storage_->save(get_mailboxes_file_name());
}

void im::save_search_history()
{
    if (search_history_)
        search_history_->save_all();
}

void im::save_cached_objects(force_save _force)
{
    save_my_info();
    save_contact_list(_force);
    save_active_dialogs();
    save_favorites();
    save_mailboxes();
    save_search_history();
}

void im::start_session(is_ping _is_ping, is_login _is_login)
{
    if (_is_ping != is_ping::yes)
    {
        wim_send_thread_->clear();
        cancel_requests();
    }

    auto wr_this = weak_from_this();
    std::string locale = g_core->get_locale();

    auto current_time = std::chrono::system_clock::now();
    auto timeout = (current_time < (start_session_time_ + start_session_timeout)) ? start_session_timeout : std::chrono::milliseconds(0);
    start_session_time_ = current_time;
    last_network_activity_time_ = current_time - dlg_state_agregate_start_timeout;

    auto packet = std::make_shared<core::wim::start_session>(
        make_wim_params(),
        _is_ping == is_ping::yes,
        g_core->get_uniq_device_id(),
        locale,
        timeout,
        [stop_objects = stop_objects_](std::chrono::milliseconds _timeout)
        {
            return stop_objects->startsession_event_.wait_for(_timeout);
        });

    post_wim_packet(packet)->on_result_ = [wr_this, packet, _is_ping, _is_login, locale](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == 0)
        {
            time_t time_offset = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) - packet->get_ts();

            ptr_this->auth_params_->aimsid_ = packet->get_aimsid();
            ptr_this->auth_params_->time_offset_ = time_offset;
            ptr_this->auth_params_->aimid_ = packet->get_aimid();
            ptr_this->auth_params_->version_ = tools::version_info().get_version();
            ptr_this->auth_params_->locale_ = locale;

            write_offset_in_log(ptr_this->auth_params_->time_offset_);

            ptr_this->fetch_params_->fetch_url_ = packet->get_fetch_url();

            im_login_id login(ptr_this->auth_params_->aimid_);
            g_core->update_login(login);
            ptr_this->set_id(login.get_id());

            ptr_this->store_auth_parameters();
            ptr_this->store_fetch_parameters();

            if (_is_ping == is_ping::yes)
            {
                ptr_this->poll(true, im::poll_reason::normal);
                return;
            }

            ptr_this->load_favorites()->on_result_ = [wr_this](int32_t _error)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                ptr_this->poll(true, im::poll_reason::normal);
            };

            ptr_this->apply_exported_settings();
        }
        else
        {
            if ((_error == wpie_error_request_canceled_wait_timeout || _error == wpie_error_too_fast_sending) && _is_login == is_login::yes)
            {
                g_core->unlogin(true);
            }
            else if (_error != wim_protocol_internal_error::wpie_error_task_canceled)
            {
                if (packet->need_relogin())
                {
                    if (_is_ping == is_ping::yes)
                        ptr_this->start_session(is_ping::no, _is_login);
                    else
                        g_core->unlogin(true);
                }
                else if (packet->need_correct_ts())
                {
                    if (packet->get_ts() > 0)
                    {
                        time_t time_offset = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) - packet->get_ts();
                        ptr_this->auth_params_->time_offset_ = time_offset;
                        ptr_this->store_auth_parameters();

                        write_offset_in_log(ptr_this->auth_params_->time_offset_);

                        ptr_this->start_session(_is_ping, _is_login);
                    }
                    else
                    {
                        g_core->unlogin(false);
                    }
                }
                else
                {
                    ptr_this->start_session(_is_ping, _is_login);
                }
            }
        }
    };
}

void im::cancel_requests()
{
    {
        std::lock_guard<std::mutex> lock(stop_objects_->session_mutex_);
        ++stop_objects_->active_session_id_;
    }

    curl_handler::instance().process_stopped_tasks();
}

bool im::is_session_valid(uint64_t _session_id)
{
    std::lock_guard<std::mutex> lock(stop_objects_->session_mutex_);
    return (_session_id == stop_objects_->active_session_id_);
}

void im::post_pending_messages(const bool _recurcive)
{
    if (sent_pending_messages_active_ && !_recurcive)
        return;

    auto wr_this = weak_from_this();

    sent_pending_messages_active_ = true;

    // stop post timer
    if (post_messages_timer_ != empty_timer_id)
    {
        g_core->stop_timer(post_messages_timer_);

        // reset timer id
        post_messages_timer_ = empty_timer_id;
    }

    // get first pending message from queue
    get_archive()->get_pending_message()->on_result = [wr_this](archive::not_sent_message_sptr _message)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        // if message queue empty
        if (!_message)
        {
            ptr_this->sent_pending_messages_active_ = false;

            return;
        }

        auto current_time = std::chrono::system_clock::now();

        // if last post message time < 1 sec
        if (current_time - _message->get_post_time() < post_messages_rate)
        {
            // start timer
            assert(ptr_this->post_messages_timer_ == empty_timer_id);

            ptr_this->post_messages_timer_ = g_core->add_timer([wr_this]
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                ptr_this->post_pending_messages(true);

            }, post_messages_rate);

            return;
        }

        ptr_this->get_archive()->update_message_post_time(_message->get_internal_id(), current_time)->on_result_ = [wr_this, _message](int32_t _error)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            ptr_this->send_pending_message_async(0, _message)->on_result_ = [wr_this](int32_t _error)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                if (_error == 0)
                {
                    ptr_this->post_pending_messages(true);
                }
                else
                {
                    ptr_this->sent_pending_messages_active_ = false;
                }
            };
        };
    };
}


void im::post_pending_delete_messages(const bool _recurcive)
{
    if (sent_pending_delete_messages_active_ && !_recurcive)
        return;

    auto wr_this = weak_from_this();

    sent_pending_delete_messages_active_ = true;

    get_archive()->get_pending_delete_message([wr_this](const bool _empty, const std::string& _contact, const archive::delete_message& _message)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        // if message queue empty
        if (_empty)
        {
            ptr_this->sent_pending_delete_messages_active_ = false;
            return;
        }


        std::shared_ptr<robusto_packet> packet;

        switch (_message.get_operation())
        {
            case archive::delete_operation::del_up_to:
                packet = std::make_shared<core::wim::del_history>(ptr_this->make_wim_params(), _message.get_message_id(), _contact);
                break;
            case archive::delete_operation::del_for_me:
                packet = std::make_shared<core::wim::del_message>(ptr_this->make_wim_params(), _message.get_message_id(), _contact, false);
                break;
            case archive::delete_operation::del_for_all:
                packet = std::make_shared<core::wim::del_message>(ptr_this->make_wim_params(), _message.get_message_id(), _contact, true);
                break;
            default:
                assert(false);
                return;
        }

        ptr_this->post_robusto_packet(packet)->on_result_ = [wr_this, _message, _contact](int32_t _error)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            if (_error == 0)
            {
                if (_message.get_operation() == archive::delete_operation::del_for_all || _message.get_operation() == archive::delete_operation::del_for_me)
                    g_core->insert_event((_message.get_operation() == archive::delete_operation::del_for_all) ? core::stats::stats_event_names::message_delete_all : core::stats::stats_event_names::message_delete_my);
            }
            else if (wim_packet::needs_to_repeat_failed(_error))
            {
                ptr_this->sent_pending_delete_messages_active_ = false;

                return;
            }

            ptr_this->get_archive()->remove_pending_delete_message(_contact, _message);

            ptr_this->post_pending_delete_messages(true);
        };
    });
}

void im::poll(
    const bool _is_first,
    const poll_reason _reason,
    const int32_t _failed_network_error_count)
{
    if (!im_created_)
        on_im_created();

    if (!fetch_params_->is_valid())
        return start_session();

    auto active_session_id = stop_objects_->active_session_id_;

    auto start_time = std::chrono::system_clock::now();

    auto stop_objects = stop_objects_;

    auto wait_function = [stop_objects](std::chrono::milliseconds _timeout)
    {
        return stop_objects->poll_event_.wait_for(_timeout);
    };

    const bool is_min_timeout = (_is_first || _reason == im::poll_reason::after_network_error || _reason == im::poll_reason::after_first_start);

    auto packet = std::make_shared<fetch>(
        make_wim_params(),
        fetch_params_->fetch_url_,
        ((is_min_timeout) ? std::chrono::milliseconds(1) : fetch_params_->next_fetch_timeout_),
        fetch_params_->next_fetch_time_,
        !ui_active_,
        std::move(wait_function));

    auto wr_this = weak_from_this();

    fetch_thread_->run_async_task(packet)->on_result_ = [_is_first, active_session_id, packet, wr_this, _failed_network_error_count, _reason](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->fetch_params_->next_fetch_time_ = packet->get_next_fetch_time();
        ptr_this->fetch_params_->next_fetch_timeout_ = packet->get_next_fetch_timeout();

        if (_error == 0)
        {
            ptr_this->check_ui_activity();

            ptr_this->dispatch_events(packet, [packet, wr_this, active_session_id, _is_first, _reason](int32_t _error)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                if (packet->need_relogin() != relogin::none)
                {
                    g_core->unlogin(packet->need_relogin() == relogin::relogin_with_error);
                    return;
                }

                auto time_offset = packet->get_time_offset();
                auto time_offset_local = packet->get_time_offset_local();

                auto time_offset_prev = ptr_this->auth_params_->time_offset_;
                auto time_offset_prev_local = ptr_this->auth_params_->time_offset_local_;

                ptr_this->fetch_params_->fetch_url_ = packet->get_next_fetch_url();

                ptr_this->fetch_params_->last_successful_fetch_ = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) + time_offset;

                ptr_this->check_need_agregate_dlg_state();
                ptr_this->last_network_activity_time_ = std::chrono::system_clock::now();

                ptr_this->store_fetch_parameters();

                if (std::chrono::seconds(packet->now() - packet->get_start_execute_time()) <= packet->timeout())
                {
                    ptr_this->auth_params_->time_offset_ = time_offset;
                    ptr_this->auth_params_->time_offset_local_ = time_offset_local;

                    write_offset_in_log(ptr_this->auth_params_->time_offset_);

                    if (
                        std::abs(time_offset - time_offset_prev) > 5 * 60 ||
                        std::abs(time_offset_local - time_offset_prev_local) > 5 * 60)
                    {
                        write_time_offests_in_log(packet);
                        ptr_this->store_auth_parameters();
                        ptr_this->send_timezone();
                    }
                }

                if (!ptr_this->is_session_valid(active_session_id))
                    return;

                im::poll_reason current_reason = im::poll_reason::normal;

                connection_state current_connecting_state = connection_state::online_from_fetch;

                if (_reason == im::poll_reason::after_first_start)
                {
                    if (packet->get_events_count() >= max_fetch_events_count)
                    {
                        current_reason = im::poll_reason::after_first_start;

                        current_connecting_state = connection_state::updating;
                    }
                }

                ptr_this->set_connection_state(current_connecting_state);

                ptr_this->poll(false, current_reason);

                ptr_this->resume_failed_network_requests();

                if (_is_first)
                {
                    g_core->post_message_to_gui("login/complete", 0, nullptr);

                    ptr_this->send_stat_if_needed();
                }
            });
        }
        else
        {
            if (_error == wpie_error_request_canceled || !ptr_this->is_session_valid(active_session_id))
                return;

            if (_error == wpie_network_error)
            {
                ptr_this->set_connection_state(connection_state::connecting);

                ptr_this->fetch_params_->next_fetch_time_ = std::chrono::system_clock::now() + std::chrono::seconds(5);

                if (_failed_network_error_count > 2)
                {
                    if (_is_first && g_core->try_to_apply_alternative_settings())
                        ptr_this->poll(_is_first, im::poll_reason::after_network_error, 0);
                    else
                        ptr_this->start_session(is_ping::yes);
                }
                else
                    ptr_this->poll(_is_first, im::poll_reason::after_network_error, (_failed_network_error_count + 1));
            }
            else if (_error == wpie_error_resend)
            {
                ptr_this->fetch_params_->next_fetch_time_ = std::chrono::system_clock::now() + std::chrono::seconds(5);

                ptr_this->poll(_is_first, im::poll_reason::resend, _failed_network_error_count);
            }
            else
            {
                ptr_this->start_session();
            }
        }
    };
}

void im::post_connection_state_to_gui(const std::string_view _state)
{
    coll_helper cl_coll(g_core->create_collection(), true);

    cl_coll.set_value_as_string("state", _state);

    g_core->post_message_to_gui("connection/state", 0, cl_coll.get());
}

void im::set_connection_state(const connection_state _state)
{
    if (active_connection_state_ != _state)
    {
        if (active_connection_state_ == connection_state::updating && _state == connection_state::online)
            return;

        active_connection_state_ = _state;

        std::string_view state_s;

        switch (_state)
        {
        case connection_state::connecting:
            state_s = "connecting";
            break;
        case connection_state::online:
        case connection_state::online_from_fetch:
            state_s = "online";
            break;
        case connection_state::updating:
            state_s = "updating";
            break;
        default:
            assert(false);
            return;
        }

        post_connection_state_to_gui(state_s);
    }
}

void im::send_stat_if_needed()
{
    const auto last_send_time = std::chrono::system_clock::from_time_t(g_core->get_last_stat_send_time());
    const auto current = std::chrono::system_clock::now();

    auto send_now = current - last_send_time > send_stat_interval;
    if (send_now)
        send_stat();

    schedule_stat_timer(!send_now);
}

void im::download_gallery_holes(const std::string& _aimid)
{
    get_archive()->get_gallery_state(_aimid)->on_result = [wr_this = weak_from_this(), _aimid](const archive::gallery_state& _local_state)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_local_state.patch_version_changed)
        {
            auto packet = std::make_shared<core::wim::get_dialog_gallery>(ptr_this->make_wim_params(), _aimid, _local_state.patch_version_, core::archive::gallery_entry_id(), core::archive::gallery_entry_id(), 0, true);
            ptr_this->post_robusto_packet(packet)->on_result_ = [wr_this, _aimid, packet](int32_t _error)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                if (_error)
                    return;

                auto new_gallery = packet->get_gallery();
                ptr_this->get_archive()->merge_gallery_from_server(_aimid, new_gallery, archive::gallery_entry_id(), archive::gallery_entry_id())->on_result = [wr_this, _aimid, new_gallery](const std::vector<archive::gallery_item>& _changes)
                {
                    auto ptr_this = wr_this.lock();
                    if (!ptr_this)
                        return;

                    if (!_changes.empty())
                    {
                        coll_helper coll(g_core->create_collection(), true);
                        ifptr<iarray> entries_array(coll->create_array());
                        entries_array->reserve((int32_t)_changes.size());
                        for (const auto& e : _changes)
                        {
                            coll_helper coll_entry(coll->create_collection(), true);
                            e.serialize(coll_entry.get());
                            ifptr<ivalue> val(coll->create_value());
                            val->set_as_collection(coll_entry.get());
                            entries_array->push_back(val.get());
                        }

                        coll.set_value_as_string("aimid", _aimid);
                        coll.set_value_as_array("entries", entries_array.get());
                        g_core->post_message_to_gui("dialog/gallery/update", 0, coll.get());
                    }

                    if (new_gallery.erased())
                    {
                        coll_helper coll(g_core->create_collection(), true);
                        coll.set_value_as_string("aimid", _aimid);
                        g_core->post_message_to_gui("dialog/gallery/erased", 0, coll.get());
                    }

                    auto state = new_gallery.get_gallery_state();
                    ptr_this->get_archive()->set_gallery_state(_aimid, state, true)->on_result_ = [wr_this, _aimid, state](int32_t _error)
                    {
                        auto ptr_this = wr_this.lock();
                        if (!ptr_this)
                            return;

                        ptr_this->post_gallery_state_to_gui(_aimid, state);
                        ptr_this->download_gallery_holes(_aimid);
                    };
                };
            };
            return;
        }

        ptr_this->get_archive()->get_gallery_holes(_aimid)->on_result = [_aimid, _local_state, wr_this](bool _result, const archive::gallery_entry_id& _from, const archive::gallery_entry_id& _till)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            if (!_result)
            {
                coll_helper coll(g_core->create_collection(), true);
                coll.set_value_as_string("aimid", _aimid);
                g_core->post_message_to_gui("dialog/gallery/holes_downloaded", 0, coll.get());
                return;
            }

            auto packet = std::make_shared<core::wim::get_dialog_gallery>(ptr_this->make_wim_params(), _aimid, _local_state.patch_version_, _from, _till, 1000);
            ptr_this->post_robusto_packet(packet)->on_result_ = [wr_this, _aimid, _local_state, _from, _till, packet](int32_t _error)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                if (_error == 0)
                {
                    auto gallery = packet->get_gallery();
                    ptr_this->get_archive()->merge_gallery_from_server(_aimid, gallery, _from, _till)->on_result = [wr_this, _aimid, _local_state, _till, gallery](const std::vector<archive::gallery_item>& _changes)
                    {
                        auto ptr_this = wr_this.lock();
                        if (!ptr_this)
                            return;

                        {
                            coll_helper coll(g_core->create_collection(), true);
                            coll.set_value_as_string("aimid", _aimid);
                            g_core->post_message_to_gui("dialog/gallery/holes_downloading", 0, coll.get());
                        }

                        if (!_till.empty() && !_changes.empty())
                        {
                            coll_helper coll(g_core->create_collection(), true);
                            ifptr<iarray> entries_array(coll->create_array());
                            entries_array->reserve((int32_t)_changes.size());
                            for (const auto& e : _changes)
                            {
                                coll_helper coll_entry(coll->create_collection(), true);
                                e.serialize(coll_entry.get());
                                ifptr<ivalue> val(coll->create_value());
                                val->set_as_collection(coll_entry.get());
                                entries_array->push_back(val.get());
                            }

                            coll.set_value_as_string("aimid", _aimid);
                            coll.set_value_as_array("entries", entries_array.get());
                            g_core->post_message_to_gui("dialog/gallery/update", 0, coll.get());
                        }

                        if (!_local_state.first_entry_.valid())
                        {
                            auto new_state = gallery.get_gallery_state();
                            ptr_this->get_archive()->set_gallery_state(_aimid, new_state, true)->on_result_ = [wr_this, _aimid, new_state](int32_t _error)
                            {
                                auto ptr_this = wr_this.lock();
                                if (!ptr_this)
                                    return;

                                ptr_this->post_gallery_state_to_gui(_aimid, new_state);

                                coll_helper coll(g_core->create_collection(), true);
                                coll.set_value_as_string("aimid", _aimid);
                                g_core->post_message_to_gui("dialog/gallery/init", 0, coll.get());
                            };
                        }

                        ptr_this->get_archive()->clear_hole_request(_aimid)->on_result_ = [_aimid, wr_this](int32_t _error)
                        {
                            auto ptr_this = wr_this.lock();
                            if (!ptr_this)
                                return;

                            ptr_this->download_gallery_holes(_aimid);
                        };
                    };
                }
                else
                {
                    ptr_this->get_archive()->clear_hole_request(_aimid);
                }
            };
        };
    };
}

void im::dispatch_events(std::shared_ptr<fetch> _fetch_packet, std::function<void(int32_t)> _on_complete)
{
    auto evt = _fetch_packet->pop_event();

    if (!evt)
    {
        if (_on_complete)
            _on_complete(0);

        return;
    }

    evt->on_im(shared_from_this(), std::make_shared<auto_callback>([_on_complete, wr_this = weak_from_this(), _fetch_packet](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->dispatch_events(_fetch_packet, _on_complete);
    }));
}


void im::logout(std::function<void()> _on_result)
{
    auto packet = std::make_shared<core::wim::end_session>(make_wim_params());

    post_wim_packet(packet)->on_result_ = [packet, _on_result](int32_t _error)
    {
        _on_result();
    };
}

void im::on_login_result(int64_t _seq, int32_t err, bool _from_exported_data)
{
    coll_helper cl_coll(g_core->create_collection(), true);

    core::login_error error_code = core::login_error::le_network_error;

    if (err != 0)
    {
        cl_coll.set_value_as_bool("result", false);

        switch ((wim_protocol_internal_error) err)
        {
            case wpie_wrong_login:
            case wpie_wrong_login_2x_factor:
            {
                error_code = core::login_error::le_wrong_login;
                if (wpie_wrong_login_2x_factor == (wim_protocol_internal_error) err)
                    error_code = core::login_error::le_wrong_login_2x_factor;

                if (_from_exported_data)
                {
                    coll_helper coll(g_core->create_collection(), true);

                    coll.set_value_as_bool("is_auth_error", true);

                    g_core->post_message_to_gui("need_login", 0, coll.get());

                    return;
                }

                break;
            }
            default:
            {
                error_code = core::login_error::le_network_error;
                break;
            }
        }

        cl_coll.set_value_as_int("error", (int32_t) error_code);
    }
    else
    {
        cl_coll.set_value_as_bool("result", true);
    }

    g_core->post_message_to_gui("login_result", _seq, cl_coll.get());
}

void im::on_login_result_attach_uin(int64_t _seq, int32_t err, const auth_parameters& auth_params, const wim_packet_params& _from_params)
{
    coll_helper cl_coll(g_core->create_collection(), true);

    core::login_error error_code = core::login_error::le_network_error;

    if (err != 0)
    {
        cl_coll.set_value_as_bool("result", false);

        switch ((wim_protocol_internal_error) err)
        {
        case wpie_wrong_login:
            {
                error_code = core::login_error::le_wrong_login;
                break;
            }
        case wpie_error_attach_busy_phone:
            {
                error_code = core::login_error::le_attach_error_busy_phone;
                break;
            }
        default:
            {
                error_code = core::login_error::le_network_error;
                break;
            }
        }

        cl_coll.set_value_as_int("error", (int32_t) error_code);
        g_core->post_message_to_gui("login_result_attach_uin", _seq, cl_coll.get());
    }
    else
    {
        attach_uin(_seq);
    }
}

void im::attach_phone(int64_t _seq, const auth_parameters& auth_params, const phone_info& _info)
{
    auto packet = std::make_shared<core::wim::attach_phone>(make_wim_params(), _info);
    auto wr_this = weak_from_this();

    post_wim_packet(packet)->on_result_ = [packet, _seq, wr_this](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        const auto status = packet->get_response_error_code();
        core::login_error error_code = core::login_error::le_network_error;
        switch ((wim_protocol_internal_error) status)
        {
        case wpie_wrong_login:
            {
                error_code = core::login_error::le_wrong_login;
                break;
            }
        case wpie_error_attach_busy_phone:
            {
                error_code = core::login_error::le_attach_error_busy_phone;
                break;
            }
        default:
            {
                error_code = core::login_error::le_network_error;
                break;
            }
        }

        coll_helper coll(g_core->create_collection(), true);
        if (_error != 0)
        {
            coll.set_value_as_bool("result", false);
            coll.set_value_as_int("error", error_code);
        }
        else
        {
            ptr_this->attach_phone_finished();
            coll.set_value_as_bool("result", true);
        }
        g_core->post_message_to_gui("login_result_attach_phone", _seq, coll.get());
    };
}

void im::merge_account(const auth_parameters& _merge_params)
{
    auto packet = std::make_shared<core::wim::merge_account>(make_wim_params(), make_wim_params_from_auth(_merge_params));

    std::wstring export_file = get_auth_parameters_filename();
    std::wstring merge_file = get_auth_parameters_filename_merge();


    post_wim_packet(packet)->on_result_ = [packet, merge_file, export_file](int32_t _error)
    {
        if (_error != wim_protocol_internal_error::wpie_network_error)
        {
            tools::system::delete_file(merge_file);

            if (tools::system::is_exist(export_file))
            {
                tools::system::delete_file(export_file);
            }
        }
    };
}

void im::merge_account()
{
    auto wr_this = weak_from_this();

    auto merge_params = std::make_shared<auth_parameters>();

    async_tasks_->run_async_function([wr_this, merge_params]()->int32_t
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return -1;

        std::wstring merge_file = ptr_this->get_auth_parameters_filename_merge();

        core::tools::binary_stream bs_merge;
        if (!bs_merge.load_from_file(merge_file))
        {
            return -1;
        }

        bs_merge.write<char>('\0');

        rapidjson::Document doc;
        if (doc.Parse(bs_merge.read_available()).HasParseError())
        {
            return -1;
        }

        if (!merge_params->unserialize(doc))
        {
            return -1;
        }

        return 0;

    })->on_result_ = [wr_this, merge_params](int32_t _error)
    {
        if (_error != 0)
            return;

        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (merge_params->password_md5_.length())
        {
            auto packet = std::make_shared<client_login>(ptr_this->make_wim_params(),
                                                         merge_params->aimid_,
                                                         merge_params->password_md5_);

            ptr_this->post_wim_packet(packet)->on_result_ = [wr_this, merge_params, packet](int32_t _error)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                if (_error == 0)
                {
                    merge_params->a_token_ = packet->get_a_token();
                    merge_params->session_key_ = packet->get_session_key();
                    merge_params->exipired_in_ = packet->get_expired_in();
                    merge_params->time_offset_ = packet->get_time_offset();
                    merge_params->aimsid_ = std::string();
                    merge_params->password_md5_ = std::string();

                    ptr_this->merge_account(*merge_params);
                }
            };
        }
        else
        {
            ptr_this->merge_account(*merge_params);
        }
    };
}


void im::apply_exported_settings()
{
    merge_account();

    apply_exported_muted_chats();
}

void im::apply_exported_muted_chats_internal()
{
    std::wstring merge_file = get_exported_muted_chats_filename();

    auto wr_this = weak_from_this();

    auto merge_params = std::make_shared<auth_parameters>();

    auto chats_list = std::make_shared<std::list<std::string>>();

    async_tasks_->run_async_function([merge_file, merge_params, chats_list]()->int32_t
    {
        core::tools::binary_stream bs_merge;

        if (!bs_merge.load_from_file(merge_file))
        {
            return -1;
        }

        bs_merge.write<char>('\0');

        rapidjson::Document doc;
        if (doc.Parse(bs_merge.read_available()).HasParseError() || !doc.IsArray())
        {
            return -1;
        }

        for (auto iter = doc.Begin(); iter != doc.End(); ++iter)
        {
            if (!iter->IsString())
                continue;

            chats_list->push_back(core::rapidjson_get_string(*iter));
        }

        return 0;

    })->on_result_ = [wr_this, merge_params, merge_file, chats_list](int32_t _error)
    {
        if (chats_list->empty())
            return;

        if (_error != 0)
            return;

        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->mute_chats(chats_list);
    };
}

void im::apply_exported_muted_chats()
{
    if (mute_chats_timer_ != 0)
        return;

    auto wr_this = weak_from_this();

    mute_chats_timer_ = g_core->add_timer([wr_this]
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->apply_exported_muted_chats_internal();

        g_core->stop_timer(ptr_this->mute_chats_timer_);

        ptr_this->mute_chats_timer_ = 0;

    }, std::chrono::seconds(2));
}

void im::attach_uin(int64_t _seq)
{
    cancel_requests();

    auto packet = std::make_shared<core::wim::attach_uin>(make_wim_params(), make_wim_params_general(false));
    auto wr_this = weak_from_this();

    post_wim_packet(packet)->on_result_ = [packet, wr_this, _seq](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == 0)
        {
            ptr_this->attach_uin_finished();
        }

        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_bool("result", _error == 0);
        coll.set_value_as_int("error", _error);
        g_core->post_message_to_gui("login_result_attach_uin", _seq, coll.get());
    };
    //g_core->post_message_to_gui("login_result_connect", 0, cl_coll.get());
}

void im::attach_uin_finished()
{
    im_login_id old_login(auth_params_->aimid_);
    old_login.set_id(get_id());
    im_login_id new_login(attached_auth_params_->aimid_);
    new_login.set_id(get_id());
    g_core->replace_uin_in_login(old_login, new_login);
    auth_params_ = attached_auth_params_;
}

void im::attach_phone_finished()
{
    phone_registration_data_ = attached_phone_registration_data_;
}

std::string im::get_contact_friendly_name(const std::string& contact_login)
{
    assert(!!contact_list_);
    if (!!contact_list_)
        return contact_list_->get_contact_friendly_name(contact_login);

    return std::string();
}

void im::post_contact_list_to_gui()
{
    ifptr<icollection> cl_coll(g_core->create_collection(), true);
    contact_list_->serialize(cl_coll.get(), std::string());

    g_core->post_message_to_gui("contactlist", 0, cl_coll.get());
}


void im::post_ignorelist_to_gui(int64_t _seq)
{
    coll_helper coll(g_core->create_collection(), true);

    contact_list_->serialize_ignorelist(coll.get());

    g_core->post_message_to_gui("contacts/get_ignore/result", _seq, coll.get());
}

void im::post_my_info_to_gui()
{
    coll_helper info_coll(g_core->create_collection(), true);
    my_info_cache_->get_info()->serialize(info_coll);

    g_core->post_message_to_gui("my_info", 0, info_coll.get());
}


void im::post_active_dialogs_to_gui()
{
    ifptr<icollection> cl_coll(g_core->create_collection(), true);
    active_dialogs_->serialize(cl_coll.get());

    g_core->post_message_to_gui("active_dialogs", 0, cl_coll.get());
}

void im::post_active_dialogs_are_empty_to_gui()
{
    ifptr<icollection> cl_coll(g_core->create_collection(), true);
    //
    g_core->post_message_to_gui("active_dialogs_are_empty", 0, cl_coll.get());
}

void im::on_event_buddies_list(fetch_event_buddy_list* _event, std::shared_ptr<auto_callback> _on_complete)
{
    if (auto contact_list = _event->get_contactlist(); contact_list)
    {
        insert_friendly(contact_list->get_persons(), friendly_source::remote);

        get_archive()->sync_with_history()->on_result_ = [contact_list, wr_this = weak_from_this(), _on_complete](int32_t _error)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            auto update_cl = [contact_list, wr_this, _on_complete](int32_t _error)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                if (const auto res = ptr_this->contact_list_->update_cl(*contact_list); !res.update_avatars_.empty())
                {
                    const auto path = ptr_this->get_avatars_data_path();
                    for (const auto& aimid : res.update_avatars_)
                    {
                        ptr_this->get_avatar_loader()->remove_contact_avatars(aimid, path)->on_result_ = [aimid](int32_t _error)
                        {
                            coll_helper coll(g_core->create_collection(), true);
                            coll.set_value_as_string("aimid", aimid);

                            g_core->post_message_to_gui("avatars/presence/updated", 0, coll.get());
                        };
                    }
                }

                ptr_this->post_contact_list_to_gui();

                _on_complete->callback(_error);
            };

            if (ptr_this->contact_list_->is_empty())
                ptr_this->load_contact_list()->on_result_ = update_cl;
            else
                update_cl(0);
        };
    }
}

void im::on_event_diff(fetch_event_diff* _event, std::shared_ptr<auto_callback> _on_complete)
{
    auto diff = _event->get_diff();

    insert_friendly(_event->get_persons(), core::friendly_source::remote);

    get_archive()->sync_with_history()->on_result_ = [diff = std::move(diff), wr_this = weak_from_this(), _on_complete](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        for (const auto& [type, cl_diff] : *diff)
        {
            auto removed = std::make_shared<std::list<std::string>>();
            ptr_this->contact_list_->merge_from_diff(type, cl_diff, removed);

            if (removed->empty() && type == "deleted")
                continue;

            for (const auto& contact : *removed)
            {
                ptr_this->active_dialogs_->remove(contact);
                ptr_this->unfavorite(contact);

                ptr_this->get_archive()->clear_dlg_state(contact)->on_result_ = [contact](int32_t _err)
                {
                    coll_helper cl_coll(g_core->create_collection(), true);
                    cl_coll.set_value_as_string("contact", contact);
                    g_core->post_message_to_gui("active_dialogs_hide", 0, cl_coll.get());
                };
            }

            ifptr<icollection> cl_coll(g_core->create_collection(), true);
            cl_diff->serialize(cl_coll.get(), type);
            g_core->post_message_to_gui("contactlist/diff", 0, cl_coll.get());
        }

        on_created_groupchat(diff);

        _on_complete->callback(_error);
    };
}

void im::on_created_groupchat(std::shared_ptr<diffs_map> _diff)
{
    for (const auto & iter : *_diff)
    {
        if (iter.first == "created")
        {
            auto group = iter.second->get_first_group();

            if (!group)
            {
                assert(false);
                continue;
            }

            if (!group->buddies_.empty())
            {
                auto buddy = *(group->buddies_.begin());

                if (!!buddy)
                {
                    coll_helper created_chat_coll(g_core->create_collection(), true);

                    created_chat_coll.set_value_as_string("aimId", buddy->aimid_);
                    g_core->post_message_to_gui("open_created_chat", 0, created_chat_coll.get());

                    return;
                }
            }
        }
    }
}

void im::on_event_my_info(fetch_event_my_info* _event, std::shared_ptr<auto_callback> _on_complete)
{
    auto info = _event->get_info();
    my_info_cache_->set_info(info);

    insert_friendly(info->get_aimid(), info->get_friendly(), info->get_nick(), info->official(), core::friendly_source::remote);
    post_my_info_to_gui();

    const auto& accept_types = info->get_user_agreement_info().need_to_accept_types_;
    bool gdpr_agreement_requested =
            std::find(accept_types.cbegin(), accept_types.cend(), wim::user_agreement_info::agreement_type::gdpr_pp)
                        != accept_types.cend();

     suppress_gdpr_resend_.store(!gdpr_agreement_requested);

    _on_complete->callback(0);

#ifdef _DEBUG
    // my info loop test
    /*static bool first = true;

    if (first)
    {
        auto test_event  = std::make_shared<fetch_event_my_info>(*_event);

        g_core->add_timer([test_event, this]()
        {
            auto callback = std::make_shared<auto_callback>([](int32_t){});

            on_event_my_info(test_event.get(), callback);

        }, std::chrono::milliseconds(10));
    }

    first = false;*/
#endif //_DEBUG
}

void im::on_event_user_added_to_buddy_list(fetch_event_user_added_to_buddy_list* _event, std::shared_ptr<auto_callback> _on_complete)
{
    _on_complete->callback(0);
}

void im::on_event_typing(fetch_event_typing *_event, std::shared_ptr<auto_callback> _on_complete)
{
    if (!_event->aim_id().length())
    {
        _on_complete->callback(0);
        return;
    }

    coll_helper coll(g_core->create_collection(), true);
    coll.set_value_as_string("aimId", _event->aim_id());
    coll.set_value_as_string("chatterAimId", _event->chatter_aim_id());
    coll.set_value_as_string("chatterName", _event->chatter_name());
    if (_event->is_typing())
        g_core->post_message_to_gui("typing", 0, coll.get());
    else
        g_core->post_message_to_gui("typing/stop", 0, coll.get());

    _on_complete->callback(0);
}

void im::on_event_presence(fetch_event_presence* _event, std::shared_ptr<auto_callback> _on_complete)
{
    auto presence = _event->get_presence();
    const auto& aimid = _event->get_aimid();

    if (presence->lastseen_ > 0)
    {
        presence->lastseen_ += int32_t(auth_params_->time_offset_);
        if (presence->lastseen_ < 0)
            presence->lastseen_ = 0;
    }

    contact_list_->update_presence(aimid, presence);

    post_presence_to_gui(aimid);

    insert_friendly(aimid, presence->friendly_, presence->nick_, presence->official_, friendly_source::remote);

    if (contact_list_->get_need_update_avatar(true))
    {
        get_avatar_loader()->remove_contact_avatars(aimid, get_avatars_data_path())->on_result_ = [aimid](int32_t _error)
        {
            coll_helper coll(g_core->create_collection(), true);
            coll.set_value_as_string("aimid", aimid);

            g_core->post_message_to_gui("avatars/presence/updated", 0, coll.get());
        };

    }

    _on_complete->callback(0);
}

std::shared_ptr<async_task_handlers> im::post_robusto_packet(std::shared_ptr<robusto_packet> _packet)
{
    return post_wim_packet(_packet);
}

std::shared_ptr<archive::face> im::get_archive()
{
    if (!archive_)
        archive_ = std::make_shared<archive::face>(get_im_data_path() + L"/archive");

    return archive_;
}

std::shared_ptr<search::search_pattern_history> im::get_search_history()
{
    if (!search_history_)
        search_history_ = std::make_shared<search::search_pattern_history>(get_search_history_path());

    return search_history_;
}

void serialize_messages_4_gui(const std::string& _value_name, const archive::history_block::const_iterator _it_start, const archive::history_block::const_iterator _it_end, icollection* _collection, const time_t _offset)
{
    coll_helper coll(_collection, false);
    ifptr<iarray> messages_array(coll->create_array());
    ifptr<iarray> deleted_array(coll->create_array());
    ifptr<iarray> modified_array(coll->create_array());

    if (_it_start != _it_end)
    {
        messages_array->reserve((int32_t)std::distance(_it_start, _it_end));

        for (const auto &message : boost::make_iterator_range(_it_start, _it_end))
        {
            if (message->is_patch())
            {
                if (message->is_deleted())
                {
                    coll_helper coll_deleted(coll->create_collection(), true);
                    message->serialize(coll_deleted.get(), _offset);

                    ifptr<ivalue> val(coll->create_value());
                    val->set_as_collection(coll_deleted.get());

                    deleted_array->push_back(val.get());
                }

                if (message->is_modified())
                {
                    coll_helper coll_modification(coll->create_collection(), true);
                    message->serialize(coll_modification.get(), _offset);

                    ifptr<ivalue> val(coll->create_value());
                    val->set_as_collection(coll_modification.get());

                    modified_array->push_back(val.get());
                }

                continue;
            }

            __LOG(core::log::info("archive", boost::format("serialize message for gui, %1%") % message->get_text());)

            coll_helper coll_message(coll->create_collection(), true);
            message->serialize(coll_message.get(), _offset);
            ifptr<ivalue> val(coll->create_value());
            val->set_as_collection(coll_message.get());
            messages_array->push_back(val.get());
        }

        coll.set_value_as_array(_value_name, messages_array.get());

        if (!deleted_array->empty())
            coll.set_value_as_array("deleted", deleted_array.get());

        if (!modified_array->empty())
            coll.set_value_as_array("modified", modified_array.get());
    }
}

void serialize_messages_4_gui(const std::string& _value_name, const std::shared_ptr<archive::history_block>& _messages, icollection* _collection, const time_t _offset)
{
    serialize_messages_4_gui(_value_name, _messages->cbegin(), _messages->cend(), _collection, _offset);
}

namespace
{
    enum class from_deleted_mode
    {
        no,
        yes
    };

    enum class skip_patches
    {
        no,
        yes
    };
}

void serialize_message_ids_4_gui(std::string_view _value_name, const std::shared_ptr<archive::history_block>& _messages, icollection* _collection, const time_t _offset, from_deleted_mode _from_deleted, skip_patches _skip_patches)
{
    coll_helper coll(_collection, false);
    ifptr<iarray> message_ids_array(coll->create_array());

    ifptr<iarray> deleted_array(coll->create_array());
    ifptr<iarray> modified_array(coll->create_array());

    if (!_messages->empty())
    {
        message_ids_array->reserve((int32_t)_messages->size());

        for (const auto &message : *_messages)
        {
            __LOG(core::log::info("archive", boost::format("serialize message id for gui, %1%") % message->get_msgid());)

            if (message->is_patch())
            {
                if (message->is_deleted())
                {
                    coll_helper coll_deleted(coll->create_collection(), true);
                    message->serialize(coll_deleted.get(), _offset);

                    ifptr<ivalue> val(coll->create_value());
                    val->set_as_collection(coll_deleted.get());

                    deleted_array->push_back(val.get());
                }
                else if (message->is_modified())
                {
                    coll_helper coll_modification(coll->create_collection(), true);
                    message->serialize(coll_modification.get(), _offset);

                    ifptr<ivalue> val(coll->create_value());
                    val->set_as_collection(coll_modification.get());

                    modified_array->push_back(val.get());
                }

                continue; //skip update patch here
            }

            ifptr<ivalue> val(coll->create_value());
            val->set_as_int64(message->get_msgid());
            message_ids_array->push_back(val.get());
        }
    }

    coll.set_value_as_array(_value_name, message_ids_array.get());

    if (!deleted_array->empty())
        coll.set_value_as_array("deleted", deleted_array.get());

    if (!modified_array->empty())
        coll.set_value_as_array("modified", modified_array.get());
}

void im::get_archive_messages(int64_t _seq, const std::string& _contact, int64_t _from, int64_t _count_early, int64_t _count_later, bool _need_prefetch, bool _first_request, bool _after_search)
{
    get_archive_messages(_seq, _contact, _from, _count_early, _count_later, 0, _need_prefetch, _first_request, _after_search);
}

void im::get_archive_messages(int64_t _seq, const std::string& _contact, int64_t _from, int64_t _count_early, int64_t _count_later, int32_t _recursion, bool _need_prefetch, bool _first_request, bool _after_search)
{
    assert(!_contact.empty());
    assert(_from >= -1);

    if (_contact.empty())
        return;

    auto wr_this = weak_from_this();

    add_opened_dialog(_contact);

    get_archive_messages_get_messages(_seq, _contact, _from, _count_early, _count_later, _recursion, _need_prefetch, _first_request, _after_search)->on_result_ =
        [wr_this, _contact, _first_request]
        (int32_t _error)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            auto skip_hist_request = [&_contact, _first_request]()
            {
                if (!_first_request)
                {
                    write_ignore_download_holes(_contact, "!_first_request");
                    return true;
                }
                if (!core::configuration::get_app_config().is_server_history_enabled())
                {
                    write_ignore_download_holes(_contact, "!is_server_history_enabled");
                    return true;
                }
                return false;
            };

            if (skip_hist_request())
                return;

            ptr_this->get_archive()->get_dlg_state(_contact)->on_result = [wr_this, _contact](const archive::dlg_state& _state)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                const get_history_params hist_params(_contact, -1, -1, -1, _state.get_history_patch_version(default_patch_version).as_string(), true, false, false, false/*_after_search*/);
                ptr_this->get_history_from_server(hist_params)->on_result_ = [wr_this, _contact](int32_t error)
                {
                    auto ptr_this = wr_this.lock();
                    if (!ptr_this)
                        return;

                    ptr_this->download_holes(_contact);
                };
            };
        };
}

static void get_persons_from_message(const archive::history_message& message, core::archive::persons_map& result)
{
    for (const auto& quote : message.get_quotes())
    {
        if (const auto& friendly = quote.get_sender_friendly(); !friendly.empty())
        {
            core::archive::person p;
            p.friendly_ = friendly;
            result.emplace(quote.get_sender(), std::move(p));
        }
    }

    for (const auto& [uin, friendly] : message.get_mentions())
    {
        if (!friendly.empty())
        {
            core::archive::person p;
            p.friendly_ = friendly;
            result.emplace(uin, std::move(p));
        }
    }

    if (const auto chat_data = message.get_chat_data())
    {
        if (const auto& friendly = chat_data->get_friendly(); !friendly.friendly_.empty())
            result.emplace(chat_data->get_sender(), friendly);
    }
}

static core::archive::persons_map get_persons_from_history_block(const archive::history_block& block)
{
    core::archive::persons_map map;
    for (const auto& val : block)
        get_persons_from_message(*val, map);
    return map;
}

std::shared_ptr<async_task_handlers> im::get_archive_messages_get_messages(
    int64_t _seq,
    const std::string& _contact,
    int64_t _from,
    int64_t _count_early,
    int64_t _count_later,
    int32_t _recursion,
    bool _need_prefetch,
    bool _first_request,
    bool _after_search)
{
    auto out_handler = std::make_shared<async_task_handlers>();
    const auto auto_handler = std::make_shared<tools::auto_scope>([out_handler]
    {
        if (out_handler->on_result_)
            out_handler->on_result_(0);
    });

    // load from local storage

    /// const auto count_with_prefetch = _count_early + (_need_prefetch ? PREFETCH_COUNT : 0);

    get_archive()->get_messages(_contact, _from, _count_early, _count_later)->on_result =
        [_seq, wr_this = weak_from_this(), _contact, _count_early, _count_later, _first_request, auto_handler, _from, _after_search]
        (std::shared_ptr<archive::history_block> _messages, archive::first_load _first_load, std::shared_ptr<archive::error_vector> _errors)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            if (!_errors->empty())
                write_files_error_in_log("get_messages", *_errors);

            if (_first_load == archive::first_load::yes || !_errors->empty())
                ptr_this->get_messages_for_update(_contact);
            else
                ptr_this->process_postponed_for_update_dialog(_contact);

            ptr_this->get_archive()->get_dlg_state(_contact)->on_result =
                [_seq, wr_this, _contact, _first_request, _messages, _from, _count_later, _count_early, _after_search, auto_handler]
            (const archive::dlg_state& _state)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                bool need_load_from_server = _after_search && _messages->empty();

                if (_after_search && !_messages->empty())
                {
                    assert((_count_early > 0 && _count_later == 0) || (_count_early == 0 && _count_later > 0));

                    if (_messages->size() == 1 && _messages->front()->get_msgid() == _from)
                    {
                        const auto need_early = _count_early > 0 && _messages->front()->get_prev_msgid() != -1;
                        const auto need_later = _count_later > 0 && _state.get_last_msgid() != _messages->front()->get_msgid();

                        if(need_early || need_later)
                            need_load_from_server = true;
                    }
                    else if (_count_later > 0)
                    {
                        for (auto i = _messages->size() - 1; i > 0; i--)
                        {
                            if ((*_messages)[i]->get_prev_msgid() != (*_messages)[i - 1]->get_msgid())
                            {
                                need_load_from_server = true;
                                break;
                            }
                        }
                    }
                }

                if (!need_load_from_server)
                {
                    auto local_persons = get_persons_from_history_block(*_messages);
                    if (!local_persons.empty())
                        ptr_this->insert_friendly(local_persons, core::friendly_source::local, friendly_add_mode::insert);

                    coll_helper coll(g_core->create_collection(), true);
                    coll.set<bool>("result", true);
                    coll.set<std::string>("contact", _contact);
                    serialize_messages_4_gui("messages", _messages, coll.get(), ptr_this->auth_params_->time_offset_);
                    coll.set<int64_t>("theirs_last_delivered", _state.get_theirs_last_delivered());
                    coll.set<int64_t>("theirs_last_read", _state.get_theirs_last_read());
                    coll.set<int64_t>("last_msg_in_index", _state.get_last_msgid());
                    coll.set<std::string>("my_aimid", ptr_this->auth_params_->aimid_);

                    const auto as = std::make_shared<tools::auto_scope>(
                        [coll, _seq]
                    {
                        g_core->post_message_to_gui("archive/messages/get/result", _seq, coll.get());
                    });

                    if (_first_request || _from == -1)
                    {
                        ptr_this->get_archive()->get_not_sent_messages(_contact)->on_result =
                            [wr_this, _contact, as, coll, auto_handler]
                        (std::shared_ptr<archive::history_block> _pending_messages, archive::first_load, std::shared_ptr<archive::error_vector>)
                        {
                            auto ptr_this = wr_this.lock();
                            if (!ptr_this)
                                return;

                            if (!_pending_messages->empty())
                            {
                                serialize_messages_4_gui(
                                    "pending_messages",
                                    _pending_messages,
                                    coll.get(),
                                    ptr_this->auth_params_->time_offset_);
                            }
                        }; // get_not_sent_messages
                    }
                }
                else
                {
                    const auto count = _count_later > 0 ? msg_context_size : (_count_early > 0 ? -msg_context_size : -1);
                    const auto from = count > 0 ? _from - 1 : _from;

                    const get_history_params hist_params(_contact, from, -1, int32_t(count), _state.get_history_patch_version(default_patch_version).as_string(), false, false, false, _after_search, _seq);
                    ptr_this->get_history_from_server(hist_params)->on_result_ = [wr_this, _contact, _from, _count_early, _count_later, _seq](int32_t _error)
                    {
                        auto ptr_this = wr_this.lock();
                        if (!ptr_this)
                            return;

                        if (_error != 0)
                        {
                            coll_helper coll(g_core->create_collection(), true);
                            coll.set<std::string>("contact", _contact);
                            coll.set<int64_t>("from", _from);
                            coll.set<int64_t>("count_early", _count_early);
                            coll.set<int64_t>("count_later", _count_later);
                            coll.set<bool>("is_network_error", wim_packet::needs_to_repeat_failed(_error) || _error == wpie_error_message_unknown);
                            g_core->post_message_to_gui("messages/load_after_search/error", _seq, coll.get());
                        }
                    };
                }
            }; // get_dlg_state
        }; // get_messages

    return out_handler;
}

void im::get_archive_messages_buddies(int64_t _seq, const std::string& _contact, std::shared_ptr<archive::msgids_list> _ids, is_updated_messages _is_updated_messages)
{
    __LOG(core::log::info("archive", boost::format("get messages buddies, contact=%1% messages count=%2%") % _contact % _ids->size());)

    auto wr_this = weak_from_this();

    get_archive()->get_messages_buddies(_contact, _ids)->on_result = [_seq, wr_this, _contact, _ids, _is_updated_messages](std::shared_ptr<archive::history_block> _messages, archive::first_load _first_load, std::shared_ptr<archive::error_vector> _errors)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (!_errors->empty())
            write_files_error_in_log("get_messages_buddies", *_errors);

        if (_first_load == archive::first_load::yes || !_errors->empty())
            ptr_this->get_messages_for_update(_contact);
        else
            ptr_this->process_postponed_for_update_dialog(_contact);

        auto local_persons = get_persons_from_history_block(*_messages);
        if (!local_persons.empty())
            ptr_this->insert_friendly(local_persons, core::friendly_source::local, friendly_add_mode::insert);

        ptr_this->get_archive()->get_dlg_state(_contact)->on_result = [_seq, wr_this, _contact, _ids, _messages, _is_updated_messages](const archive::dlg_state& _state)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            coll_helper coll(g_core->create_collection(), true);
            coll.set_value_as_bool("result", _messages->size() == _ids->size() && !_ids->empty());
            coll.set_value_as_string("contact", _contact);
            serialize_messages_4_gui("messages", _messages, coll.get(), ptr_this->auth_params_->time_offset_);
            coll.set_value_as_int64("theirs_last_delivered", _state.get_theirs_last_delivered());
            coll.set_value_as_int64("theirs_last_read", _state.get_theirs_last_read());
            coll.set<int64_t>("last_msg_in_index", _state.get_last_msgid());
            coll.set<std::string>("my_aimid", ptr_this->auth_params_->aimid_);
            coll.set_value_as_bool("updated", is_updated_messages::yes == _is_updated_messages);
            g_core->post_message_to_gui("archive/buddies/get/result", _seq, coll.get());
        };
    };
}

void im::get_messages_for_update(const std::string& _contact)
{
    get_archive()->get_messages_for_update(_contact)->on_result = [wr_this = weak_from_this(), _contact](std::shared_ptr<std::vector<int64_t>> _ids)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (!(_ids->empty()))
        {
            ptr_this->get_archive()->get_dlg_state(_contact)->on_result = [wr_this, _ids, _contact](const archive::dlg_state& _state)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;
                ptr_this->get_history_from_server(_contact, _state.get_history_patch_version().as_string(), *_ids)->on_result_ = [wr_this, _contact](int32_t _error)
                {
                    if (wim_packet::needs_to_repeat_failed(_error))
                    {
                        auto ptr_this = wr_this.lock();
                        if (!ptr_this)
                            return;
                        ptr_this->failed_update_messages_.insert(_contact);
                    }
                };
            };
        }
    };
}

void im::get_mentions(int64_t _seq, std::string_view _contact)
{
    get_archive()->get_mentions(_contact)->on_result = [_seq, wr_this = weak_from_this(), _contact = std::string(_contact)](std::shared_ptr<archive::history_block> _messages, archive::first_load _first_load)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_first_load == archive::first_load::yes)
            ptr_this->add_postponed_for_update_dialog(_contact);

        ptr_this->get_archive()->get_dlg_state(_contact)->on_result = [_seq, wr_this, _contact, _messages](const archive::dlg_state& _state)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            if (_state.has_last_read_mention())
            {
                const auto last_read_mention = _state.get_last_read_mention();
                // _messages are actually not shared, so we can modify
                _messages->erase(std::remove_if(_messages->begin(), _messages->end(), [last_read_mention](const auto& x) { return x->get_msgid() <= last_read_mention; }), _messages->end());
            }

            coll_helper coll(g_core->create_collection(), true);
            coll.set_value_as_bool("result", !_messages->empty());
            coll.set_value_as_string("contact", _contact);
            coll.set_value_as_string("my_aimid", ptr_this->auth_params_->aimid_);
            coll.set_value_as_int64("theirs_last_delivered", _state.get_theirs_last_delivered());
            coll.set_value_as_int64("theirs_last_read", _state.get_theirs_last_read());
            coll.set_value_as_int64("last_msg_in_index", _state.get_last_msgid());
            serialize_messages_4_gui("messages", _messages, coll.get(), ptr_this->auth_params_->time_offset_);
            g_core->post_message_to_gui("archive/mentions/get/result", _seq, coll.get());
        };
    };
}

std::shared_ptr<async_task_handlers> im::get_history_from_server(const get_history_params& _params)
{
    auto out_handler = std::make_shared<async_task_handlers>();
    auto packet = std::make_shared<core::wim::get_history>(make_wim_params(), _params, g_core->get_locale());
    const auto from_msgid = _params.from_msg_id_;

    auto wr_this = weak_from_this();

    assert(!_params.aimid_.empty());

    auto on_result = [out_handler](int32_t _error)
        {
            if (out_handler->on_result_)
                out_handler->on_result_(_error);
        };

    const auto init = _params.init_;
    const auto from_deleted = _params.from_deleted_;
    const auto from_editing = _params.from_editing_;
    const auto from_search = _params.from_search_;
    const auto seq = _params.seq_;

    post_robusto_packet(packet)->on_result_ = [wr_this, packet, contact = _params.aimid_, init, from_deleted, from_editing, from_search, on_result, from_msgid, seq](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error != 0)
        {
            on_result(_error);
            return;
        }

        const auto new_dlg_state = packet->get_dlg_state();

        const auto messages = packet->get_messages();
        const auto unpin = packet->get_unpinned();
        const auto has_older_msgid = packet->has_older_msgid();

        ptr_this->insert_friendly(packet->get_persons(), friendly_source::remote);

        __INFO(
            "delete_history",
            "processing incoming history from a server\n"
            "    contact=<%1%>\n"
            "    history-patch=<%2%>\n"
            "    messages-size=<%3%>",
            contact % new_dlg_state->get_history_patch_version().as_string() % messages->size());


        ptr_this->get_archive()->get_dlg_state(contact)->on_result =
            [wr_this, contact, messages, init, from_deleted, from_editing, from_search, new_dlg_state, on_result, unpin, from_msgid, has_older_msgid, seq](const archive::dlg_state& _local_state)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            const bool is_tail = from_msgid == -1;

            new_dlg_state->set_official(_local_state.get_official());
            new_dlg_state->set_friendly(_local_state.get_friendly());
            new_dlg_state->set_suspicious(_local_state.get_suspicious());
            new_dlg_state->set_stranger(_local_state.get_stranger());
            new_dlg_state->set_last_read_mention(_local_state.get_last_read_mention());

            if (is_tail && !messages->empty())
            {
                for (const auto& _message : *messages)
                {
                    if (!_message->is_deleted() && !_message->is_patch() && !_message->is_modified())
                    {
                        new_dlg_state->set_last_message(*_message);

                        break;
                    }
                }
            }

            if (!new_dlg_state->has_pinned_message() && !unpin)
                new_dlg_state->set_pinned_message(_local_state.get_pinned_message());

            const auto pin_changed =
                (new_dlg_state->has_pinned_message() && !_local_state.has_pinned_message()) ||
                 new_dlg_state->get_pinned_message().get_msgid() != _local_state.get_pinned_message().get_msgid() ||
                 new_dlg_state->get_pinned_message().get_update_patch_version() > _local_state.get_pinned_message().get_update_patch_version();

            ptr_this->get_archive()->set_dlg_state(contact, *new_dlg_state)->on_result =
                [wr_this, contact, messages, init, from_deleted, from_editing, from_search, is_tail, on_result, pin_changed, from_msgid, has_older_msgid, seq]
                (const archive::dlg_state& _state, const archive::dlg_state_changes& _changes)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                if (std::any_of(messages->cbegin(), messages->cend(), [](const auto& x) { return x->is_patch() && x->is_clear(); }))
                {
                    ptr_this->get_archive()->drop_history(contact)->on_result_ = [wr_this, contact](int32_t _error)
                    {
                        auto ptr_this = wr_this.lock();
                        if (!ptr_this)
                            return;

                        coll_helper coll(g_core->create_collection(), true);
                        coll.set<bool>("result", true);
                        coll.set<std::string>("contact", contact);
                        g_core->post_message_to_gui("messages/clear", 0, coll.get());

                        if (ptr_this->has_opened_dialogs(contact))
                            ptr_this->download_holes(contact);
                    };
                    return;
                }

                remove_messages_from_not_sent(ptr_this->get_archive(), contact, false, messages)->on_result_ =
                    [wr_this, contact, messages, init, from_deleted, from_editing, from_search, is_tail, on_result, seq, _state, _changes, pin_changed, from_msgid, has_older_msgid]
                    (int32_t _error)
                {
                    auto ptr_this = wr_this.lock();
                    if (!ptr_this)
                        return;

                    ptr_this->get_archive()->update_history(contact, messages, from_editing ? -1 : from_msgid, has_older_msgid ? archive::local_history::has_older_message_id::yes : archive::local_history::has_older_message_id::no)->on_result =
                        [wr_this, contact, on_result, _changes, messages, pin_changed, _state, from_deleted, from_editing, from_search, init, is_tail, seq]
                        (std::shared_ptr<archive::headers_list> _inserted_messages, const archive::dlg_state&, const archive::dlg_state_changes& _changes_on_update, core::archive::storage::result_type _result)
                    {
                        auto ptr_this = wr_this.lock();
                        if (!ptr_this)
                            return;

                        if (!messages->empty() && ptr_this->has_opened_dialogs(contact))
                        {
                            coll_helper coll(g_core->create_collection(), true);
                            coll.set<bool>("result", true);
                            coll.set<std::string>("contact", contact);
                            coll.set<int64_t>("theirs_last_delivered", _state.get_theirs_last_delivered());
                            coll.set<int64_t>("theirs_last_read", _state.get_theirs_last_read());
                            coll.set<int64_t>("last_msg_in_index", _state.get_last_msgid());
                            coll.set<std::string>("my_aimid", ptr_this->auth_params_->aimid_);
                            if (init || is_tail)
                            {
                                serialize_messages_4_gui("messages", messages, coll.get(), ptr_this->auth_params_->time_offset_);
                                g_core->post_message_to_gui(init ? "messages/received/init" : "messages/received/dlg_state", seq, coll.get()); // post last message as dlg_state
                            }
                            else if (from_search)
                            {
                                serialize_messages_4_gui("messages", messages, coll.get(), ptr_this->auth_params_->time_offset_);
                                g_core->post_message_to_gui("messages/received/search_mode", seq, coll.get());
                            }
                            else if (from_editing)
                            {
                                serialize_message_ids_4_gui("ids", messages, coll.get(), ptr_this->auth_params_->time_offset_,
                                    from_deleted ? from_deleted_mode::yes : from_deleted_mode::no, skip_patches::yes);
                                g_core->post_message_to_gui("messages/received/updated", seq, coll.get());
                            }
                            else
                            {
                                serialize_message_ids_4_gui("ids", messages, coll.get(), ptr_this->auth_params_->time_offset_,
                                    from_deleted ? from_deleted_mode::yes : from_deleted_mode::no, skip_patches::no);
                                g_core->post_message_to_gui("messages/received/server", seq, coll.get());
                            }
                        }

                        if (!_result)
                            write_files_error_in_log("update_history", _result.error_code_);

                        ptr_this->post_outgoing_count_to_gui(contact);

                        const auto last_message_changed = (_changes.last_message_changed_ && !_changes.initial_fill_);

                        const auto last_message_changed_on_update = _changes_on_update.last_message_changed_;

                        const auto skip_dlg_state = (!last_message_changed_on_update && !last_message_changed);

                        std::vector<int64_t> updated_ids;
                        for (const auto& header : *_inserted_messages)
                        {
                            if (header.is_updated_message())
                                updated_ids.push_back(header.get_id());
                        }

                        if (skip_dlg_state && !pin_changed)
                        {
                            if (!updated_ids.empty())
                            {
                                ptr_this->get_history_from_server(contact, _state.get_history_patch_version().as_string(), std::move(updated_ids))->on_result_ = [on_result](int32_t /*_error*/)
                                {
                                    on_result(0);
                                };
                            }
                            else
                            {
                                on_result(0);
                            }
                        }
                        else
                        {
                            ptr_this->post_dlg_state_to_gui(contact)->on_result_ = [on_result, wr_this, contact, _state, ids = std::move(updated_ids)](int32_t /*_error*/)
                            {
                                auto ptr_this = wr_this.lock();
                                if (!ptr_this)
                                    return;
                                if (!ids.empty())
                                {
                                    ptr_this->get_history_from_server(contact, _state.get_history_patch_version().as_string(), std::move(ids))->on_result_ = [on_result](int32_t /*_error*/)
                                    {
                                        on_result(0);
                                    };
                                }
                                else
                                {
                                    on_result(0);
                                }
                            };
                        }
                    };
                }; // remove_messages_from_not_sent
            }; // set_dlg_state
        }; //get_dlg_state
    }; // post_robusto_packet

    return out_handler;
}

std::shared_ptr<async_task_handlers> im::get_history_from_server(const std::string_view _contact, const std::string_view _patch_version, std::vector<int64_t> _ids)
{
    get_history_batch_params hist_params(_contact, std::move(_ids), _patch_version);
    return get_history_from_server_batch(hist_params);
}

std::shared_ptr<async_task_handlers> im::get_history_from_server(const std::string_view _contact, const std::string_view _patch_version, const int64_t _id, const int32_t _context_early, const int32_t _context_after, int64_t _seq)
{
    get_history_batch_params hist_params(_contact, _patch_version, _id, _context_early, _context_after, _seq);
    return get_history_from_server_batch(hist_params);
}

std::shared_ptr<async_task_handlers> im::get_history_from_server_batch(get_history_batch_params _params)
{
    assert(!_params.aimid_.empty());
    assert(!_params.ids_.empty());
    assert(!_params.patch_version_.empty());

    auto out_handler = std::make_shared<async_task_handlers>();
    auto on_result = [out_handler](int32_t _error)
    {
        if (out_handler->on_result_)
            out_handler->on_result_(_error);
    };

    const auto contact = _params.aimid_;
    const auto is_context_req = _params.is_message_context_params();
    const auto seq = _params.seq_;
    const auto msg_id = _params.ids_.empty() ? -1 : _params.ids_.back();

    auto packet = std::make_shared<core::wim::get_history_batch>(make_wim_params(), std::move(_params), g_core->get_locale());
    post_robusto_packet(packet)->on_result_ = [wr_this = weak_from_this(), packet, contact, on_result, is_context_req, seq, msg_id](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error != 0)
        {
            on_result(_error);
            return;
        }

        const auto new_dlg_state = packet->get_dlg_state();

        const auto messages = packet->get_messages();
        const auto unpin = packet->get_unpinned();

        ptr_this->insert_friendly(packet->get_persons(), friendly_source::remote);

        ptr_this->get_archive()->get_dlg_state(contact)->on_result =
            [wr_this, contact, messages, new_dlg_state, on_result, unpin, is_context_req, seq, msg_id](const archive::dlg_state& _local_state)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            new_dlg_state->set_official(_local_state.get_official());
            new_dlg_state->set_friendly(_local_state.get_friendly());
            new_dlg_state->set_suspicious(_local_state.get_suspicious());
            new_dlg_state->set_stranger(_local_state.get_stranger());

            if (!new_dlg_state->has_pinned_message() && !unpin)
                new_dlg_state->set_pinned_message(_local_state.get_pinned_message());

            const auto pin_changed =
                (new_dlg_state->has_pinned_message() && !_local_state.has_pinned_message()) ||
                 new_dlg_state->get_pinned_message().get_msgid() != _local_state.get_pinned_message().get_msgid() ||
                 new_dlg_state->get_pinned_message().get_update_patch_version() > _local_state.get_pinned_message().get_update_patch_version();

            ptr_this->get_archive()->set_dlg_state(contact, *new_dlg_state)->on_result =
                [wr_this, contact, messages, on_result, pin_changed, is_context_req, seq, msg_id]
            (const archive::dlg_state& _state, const archive::dlg_state_changes& _changes)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                if (std::any_of(messages->cbegin(), messages->cend(), [](const auto& x) { return x->is_patch() && x->is_clear(); }))
                {
                    ptr_this->get_archive()->drop_history(contact)->on_result_ = [wr_this, contact](int32_t _error)
                    {
                        auto ptr_this = wr_this.lock();
                        if (!ptr_this)
                            return;

                        coll_helper coll(g_core->create_collection(), true);
                        coll.set<bool>("result", true);
                        coll.set<std::string>("contact", contact);
                        g_core->post_message_to_gui("messages/clear", 0, coll.get());

                        if (ptr_this->has_opened_dialogs(contact))
                            ptr_this->download_holes(contact);
                    };
                    return;
                }

                remove_messages_from_not_sent(ptr_this->get_archive(), contact, false, messages)->on_result_ =
                    [wr_this, contact, messages, on_result, _state, _changes, pin_changed, is_context_req, seq, msg_id]
                (int32_t _error)
                {
                    auto ptr_this = wr_this.lock();
                    if (!ptr_this)
                        return;

                    ptr_this->get_archive()->update_history(contact, messages)->on_result =
                        [wr_this, contact, on_result, _changes, messages, pin_changed, _state, is_context_req, seq, msg_id]
                    (std::shared_ptr<archive::headers_list> _inserted_messages, const archive::dlg_state&, const archive::dlg_state_changes& _changes_on_update, core::archive::storage::result_type _result)
                    {
                        auto ptr_this = wr_this.lock();
                        if (!ptr_this)
                            return;

                        if (!messages->empty() && ptr_this->has_opened_dialogs(contact))
                        {
                            if (is_context_req)
                            {
                                ptr_this->get_archive()->get_messages(contact, msg_id, msg_context_size / 2, msg_context_size / 2 + 1)->on_result =
                                    [wr_this, contact, _state, seq](std::shared_ptr<archive::history_block> _hist_block, archive::first_load _first_load, std::shared_ptr<archive::error_vector> _errors)
                                {
                                    auto ptr_this = wr_this.lock();
                                    if (!ptr_this)
                                        return;
                                    coll_helper coll(g_core->create_collection(), true);
                                    coll.set<bool>("result", true);
                                    coll.set<std::string>("contact", contact);
                                    coll.set<int64_t>("theirs_last_delivered", _state.get_theirs_last_delivered());
                                    coll.set<int64_t>("theirs_last_read", _state.get_theirs_last_read());
                                    coll.set<int64_t>("last_msg_in_index", _state.get_last_msgid());
                                    coll.set<std::string>("my_aimid", ptr_this->auth_params_->aimid_);
                                    serialize_messages_4_gui("messages", _hist_block, coll.get(), ptr_this->auth_params_->time_offset_);

                                    g_core->post_message_to_gui("messages/received/context", seq, coll.get());
                                };
                            }
                            else
                            {
                                coll_helper coll(g_core->create_collection(), true);
                                coll.set<bool>("result", true);
                                coll.set<std::string>("contact", contact);
                                coll.set<int64_t>("theirs_last_delivered", _state.get_theirs_last_delivered());
                                coll.set<int64_t>("theirs_last_read", _state.get_theirs_last_read());
                                coll.set<int64_t>("last_msg_in_index", _state.get_last_msgid());
                                coll.set<std::string>("my_aimid", ptr_this->auth_params_->aimid_);
                                serialize_message_ids_4_gui("ids", messages, coll.get(), ptr_this->auth_params_->time_offset_, from_deleted_mode::no, skip_patches::yes);
                                g_core->post_message_to_gui("messages/received/updated", seq, coll.get());
                            }
                        }

                        if (!_result)
                            write_files_error_in_log("update_history", _result.error_code_);

                        ptr_this->post_outgoing_count_to_gui(contact);

                        const auto last_message_changed = (_changes.last_message_changed_ && !_changes.initial_fill_);
                        const auto last_message_changed_on_update = _changes_on_update.last_message_changed_;
                        const auto skip_dlg_state = (!last_message_changed_on_update && !last_message_changed);

                        std::vector<int64_t> updated_ids;
                        for (const auto& header : *_inserted_messages)
                        {
                            if (header.is_updated())
                                updated_ids.push_back(header.get_id());
                        }

                        if (skip_dlg_state && !pin_changed)
                        {
                            if (!updated_ids.empty())
                            {
                                ptr_this->get_history_from_server(contact, _state.get_history_patch_version().as_string(), std::move(updated_ids))->on_result_ = [wr_this, on_result, contact](int32_t _error)
                                {
                                    if (wim_packet::needs_to_repeat_failed(_error))
                                    {
                                        auto ptr_this = wr_this.lock();
                                        if (!ptr_this)
                                            return;
                                        ptr_this->failed_update_messages_.insert(contact);
                                    }
                                    on_result(0);
                                };
                            }
                            else
                            {
                                on_result(0);
                            }
                        }
                        else
                        {
                            ptr_this->post_dlg_state_to_gui(contact)->on_result_ = [on_result, wr_this, contact, _state, ids = std::move(updated_ids)](int32_t /*_error*/)
                            {
                                auto ptr_this = wr_this.lock();
                                if (!ptr_this)
                                    return;
                                if (!ids.empty())
                                {
                                    ptr_this->get_history_from_server(contact, _state.get_history_patch_version().as_string(), std::move(ids))->on_result_ = [wr_this, on_result, contact](int32_t _error)
                                    {
                                        if (wim_packet::needs_to_repeat_failed(_error))
                                        {
                                            auto ptr_this = wr_this.lock();
                                            if (!ptr_this)
                                                return;
                                            ptr_this->failed_update_messages_.insert(contact);
                                        }
                                        on_result(0);
                                    };
                                }
                                else
                                {
                                    on_result(0);
                                }
                            };
                        }
                    };
                }; // remove_messages_from_not_sent
            }; // set_dlg_state
        }; //get_dlg_state
    }; // post_robusto_packet

    return out_handler;
}

void im::get_message_context(const int64_t _seq, const std::string& _contact, const int64_t _msgid)
{
    assert(!_contact.empty());
    assert(_msgid >= 0);
    if (_contact.empty() || _msgid < 0)
        return;

    add_opened_dialog(_contact);

    auto wr_this = weak_from_this();

    get_archive()->get_dlg_state(_contact)->on_result = [wr_this, _contact, _msgid, _seq]
    (const archive::dlg_state& _state)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->get_archive()->get_messages(_contact, _msgid, msg_context_size / 2, msg_context_size / 2 + 1)->on_result =
            [wr_this, _msgid, _contact, _state, _seq](std::shared_ptr<archive::history_block> _hist_block, archive::first_load _first_load, std::shared_ptr<archive::error_vector> _errors)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            if (!_errors->empty())
                write_files_error_in_log("get_messages", *_errors);

            if (_first_load == archive::first_load::yes || !_errors->empty())
                ptr_this->get_messages_for_update(_contact);
            else
                ptr_this->process_postponed_for_update_dialog(_contact);

            const auto request_server = [&ptr_this, wr_this, _msgid, &_state, &_contact, _seq](int32_t _count_before, int32_t _count_after)
            {
                ptr_this->get_history_from_server(_contact, _state.get_history_patch_version(default_patch_version).as_string(), _msgid, _count_before, _count_after, _seq)->on_result_ = [wr_this, _contact, _msgid, _seq](int32_t _error)
                {
                    auto ptr_this = wr_this.lock();
                    if (!ptr_this)
                        return;

                    if (_error != 0)
                    {
                        coll_helper coll(g_core->create_collection(), true);
                        coll.set<std::string>("contact", _contact);
                        coll.set<int64_t>("id", _msgid);
                        coll.set<bool>("is_network_error", wim_packet::needs_to_repeat_failed(_error) || _error == wpie_error_message_unknown);
                        g_core->post_message_to_gui("messages/context/error", _seq, coll.get());
                    }
                };
            };

            constexpr auto context_half = msg_context_size / 2;

            if (!_hist_block || _hist_block->empty())
            {
                request_server(context_half, context_half);
                return;
            }

            auto& messages = *_hist_block;
            const auto center_msg_it = std::find_if(messages.begin(), messages.end(), [_msgid](const auto& msg) { return msg->get_msgid() == _msgid; });
            if (center_msg_it == messages.end())
            {
                request_server(context_half, context_half);
                return;
            }

            int existing_before = 0;
            int existing_after = 0;
            const auto center_index = std::distance(messages.begin(), center_msg_it);

            for (int i = center_index - 1; i >= 0 && existing_before < context_half; --i)
            {
                if (messages[i + 1]->get_prev_msgid() != messages[i]->get_msgid())
                    break;
                ++existing_before;
            }

            for (size_t i = center_index + 1; i < messages.size() && existing_after < context_half; ++i)
            {
                if (messages[i]->get_prev_msgid() != messages[i - 1]->get_msgid())
                    break;
                ++existing_after;
            }
            const auto after_includes_last = _state.has_last_msgid() && size_t(center_index + existing_after) < messages.size() && messages[center_index + existing_after]->get_msgid() == _state.get_last_msgid();
            const auto before_includes_first = center_index - existing_before >= 0 && messages[center_index - existing_before]->get_prev_msgid() == -1;
            const auto need_req_after = existing_after != context_half && !after_includes_last;
            const auto need_req_before = existing_before != context_half && !before_includes_first;

            if (need_req_before || need_req_after)
            {
                const auto early = existing_before == context_half ? 0 : context_half;
                const auto after = existing_after == context_half ? 0 : context_half;
                request_server(early, after);
                return;
            }

            static_assert(msg_context_size >= prefetch_count, "msg_context_size must be >= prefetch_count");
            const auto it_start  = std::prev(center_msg_it, std::min((int)prefetch_count, existing_before));
            const auto it_end    = std::next(center_msg_it, std::min((int)prefetch_count, existing_after) + 1);

            const auto local_persons = get_persons_from_history_block(messages);
            if (!local_persons.empty())
                ptr_this->insert_friendly(local_persons, core::friendly_source::local);

            coll_helper coll(g_core->create_collection(), true);
            coll.set<bool>("result", true);
            coll.set<std::string>("contact", _contact);
            coll.set<int64_t>("theirs_last_delivered", _state.get_theirs_last_delivered());
            coll.set<int64_t>("theirs_last_read", _state.get_theirs_last_read());
            coll.set<int64_t>("last_msg_in_index", _state.get_last_msgid());
            coll.set<std::string>("my_aimid", ptr_this->auth_params_->aimid_);
            serialize_messages_4_gui("messages", it_start, it_end, coll.get(), ptr_this->auth_params_->time_offset_);
            g_core->post_message_to_gui("archive/messages/get/result", _seq, coll.get());
        };
    };
}

std::shared_ptr<async_task_handlers> im::set_dlg_state(set_dlg_state_params _params)
{
    auto out_handler = std::make_shared<async_task_handlers>();

    std::string aimid = _params.aimid_;

    auto packet = std::make_shared<core::wim::set_dlg_state>(make_wim_params(), std::move(_params));

    post_robusto_packet(packet)->on_result_ = [out_handler, wr_this = weak_from_this(), aimid = std::move(aimid)](int32_t _error) mutable
    {
        if (wim_packet::needs_to_repeat_failed(_error))
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            ptr_this->failed_dlg_states_.insert(std::move(aimid));
        }

        if (out_handler->on_result_)
            out_handler->on_result_(_error);
    };

    return out_handler;
}

void im::download_failed_holes()
{
    while (auto req = failed_holes_requests_->get())
    {
        download_holes(
            req->get_contact(),
            req->get_from(),
            req->get_depth(),
            req->get_recursion());
    }
}

void im::download_holes(const std::string& _contact)
{
    im::download_holes(_contact, -1);
}

void im::download_holes(const std::string& _contact, int64_t _depth)
{
    assert(!_contact.empty());

    if (!core::configuration::get_app_config().is_server_history_enabled())
    {
        write_ignore_download_holes(_contact, "is_server_history_enabled");
        return;
    }

    auto wr_this = weak_from_this();

    get_archive()->get_dlg_state(_contact)->on_result =
        [wr_this, _contact, _depth](const archive::dlg_state& _state)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            const auto id = _state.has_last_msgid() ? _state.get_last_msgid() : -1;

            ptr_this->download_holes(_contact, id, _depth, 0);
        };
}


void im::download_holes(const std::string& _contact, int64_t _from, int64_t _depth, int32_t _recursion)
{
    __INFO("archive", "im::download_holes, contact=%1%", _contact);

    auto wr_this = weak_from_this();

    holes::request hole_request(_contact, _from, _depth, _recursion);

    get_archive()->get_next_hole(_contact, _from, _depth)->on_result =
        [wr_this, _contact, _depth, _recursion, hole_request](std::shared_ptr<archive::archive_hole> _hole)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (!_hole)
        {
            write_ignore_download_holes(_contact, "!_hole");
            return;
        }

        // fix for empty dialog
        if ((_recursion > 0) && (_hole->get_from() == -1))
        {
            write_ignore_download_holes(_contact, "recursion > 0");
            return;
        }

        // on the off-chance
        const auto safety_limit_reached = (_recursion > 30000);
        if (safety_limit_reached)
        {
            write_files_error_in_log("download_holes", _recursion);
            write_ignore_download_holes(_contact, "safety_limit_reached");
            assert(!"archive logic bug");
            return;
        }

        ptr_this->get_archive()->get_dlg_state(_contact)->on_result = [wr_this, _hole, _contact, _depth, _recursion, hole_request](const archive::dlg_state& _state)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            const auto is_from_obsolete = (_hole->has_from() && (_hole->get_from() <= _state.get_del_up_to()));
            const auto is_to_obsolete = (_hole->has_to() && (_hole->get_to() <= _state.get_del_up_to()));

            const auto is_request_obsolete = (is_from_obsolete || is_to_obsolete);
            if (is_request_obsolete)
            {
                __INFO(
                    "delete_history",
                    "obsolete holes request cancelled\n"
                    "    from=<%1%>\n"
                    "    from_obsolete=<%2%>\n"
                    "    to=<%3%>\n"
                    "    to_obsolete=<%4%>\n"
                    "    del_up_to=<%5%>",
                    _hole->get_from() % logutils::yn(is_from_obsolete) %
                    _hole->get_to() % logutils::yn(is_to_obsolete) %
                    _state.get_del_up_to()
                );

                write_ignore_download_holes(_contact, "is_request_obsolete");
                return;
            }

            constexpr int64_t default_page_size = feature::default_history_request_page_size();
            const auto page_size = std::max(omicronlib::_o("history_request_page_size", default_page_size), decltype(default_page_size)(1));

            const auto count = (-1 * page_size);
            const auto till_msg_id = (_hole->has_to() ? _hole->get_to() : -1);

            get_history_params hist_params(
                _contact,
                _hole->get_from(),
                till_msg_id,
                count,
                _state.get_history_patch_version(default_patch_version).as_string(),
                false);

            int64_t depth_tail = -1;
            if (_depth != -1)
            {
                depth_tail = (_depth - _hole->get_depth());

                if (depth_tail <= 0)
                {
                    write_ignore_download_holes(_contact, "depth_tail");
                    return;
                }

                depth_tail += page_size + 1;
            }

            ptr_this->get_history_from_server(hist_params)->on_result_ = [wr_this, _contact, _hole, depth_tail, _recursion, hole_request, count](int32_t _error)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                if (_error == 0)
                {
                    ptr_this->get_archive()->validate_hole_request(_contact, *_hole, count)->on_result = [wr_this, _contact, depth_tail, _recursion](int64_t _from)
                    {
                        auto ptr_this = wr_this.lock();
                        if (!ptr_this)
                            return;

                        ptr_this->download_holes(_contact, _from, depth_tail, _recursion + 1);

                        return;

                    }; // validate_hole_request
                }

                if (wim_packet::needs_to_repeat_failed(_error))
                {
                    ptr_this->failed_holes_requests_->add(hole_request);
                }

            }; // get_history_from_server

        }; // get_dlg_state

    }; // get_next_hole
}

void im::update_active_dialogs(const std::string& _aimid, archive::dlg_state& _state)
{
    active_dialogs_->update(active_dialog(_aimid));
}

std::shared_ptr<async_task_handlers> im::post_dlg_state_to_gui(
    const std::string& _contact,
    const bool _add_to_active_dialogs,
    const bool _serialize_message,
    const bool _force,
    const bool _load_from_local)
{
    auto handler = std::make_shared<async_task_handlers>();

    get_archive()->get_dlg_state(_contact)->on_result =
        [wr_this = weak_from_this(), handler, _contact, _add_to_active_dialogs, _serialize_message, _force, _load_from_local]
        (const archive::dlg_state& _state)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            if (_state.get_hidden_msg_id() >= _state.get_last_msgid() && !_force)
                return;

            coll_helper cl_coll(g_core->create_collection(), true);
            cl_coll.set<std::string>("contact", _contact);
            cl_coll.set<std::string>("my_aimid", ptr_this->auth_params_->aimid_);
            cl_coll.set<bool>("is_chat", _contact.find("@chat.agent") != _contact.npos);

            if (ptr_this->favorites_->contains(_contact))
            {
                cl_coll.set<int64_t>("favorite_time", ptr_this->favorites_->get_time(_contact));
                if (_add_to_active_dialogs && !ptr_this->active_dialogs_->contains(_contact))
                {
                    active_dialog dlg(_contact);
                    ptr_this->active_dialogs_->update(dlg);
                }
            }

            _state.serialize(
                cl_coll.get(),
                ptr_this->auth_params_->time_offset_,
                ptr_this->fetch_params_->last_successful_fetch_,
                _serialize_message
            );

            if (_load_from_local)
            {
                archive::persons_map persons;

                if (_state.has_last_message())
                    get_persons_from_message(_state.get_last_message(), persons);
                if (_state.has_pinned_message())
                    get_persons_from_message(_state.get_pinned_message(), persons);

                if (!_state.get_friendly().empty())
                {
                    archive::person p;
                    p.friendly_ = _state.get_friendly();
                    persons.emplace(_contact, std::move(p));
                }

                ptr_this->insert_friendly(persons, core::friendly_source::local, core::friendly_add_mode::insert);
            }

            ptr_this->post_dlg_state_to_gui(_contact, cl_coll.get());

            if (handler->on_result_)
                handler->on_result_(0);
        };

    return handler;
}


void im::sync_message_with_dlg_state_queue(std::string_view _message, const int64_t _seq, coll_helper _data)
{
    check_need_agregate_dlg_state();

    if (dlg_state_agregate_mode_)
    {
        gui_messages_queue_.emplace_back(std::string(_message), _seq, _data);
    }
    else
    {
        g_core->post_message_to_gui(_message, _seq, _data.get());
    }
}

void im::check_need_agregate_dlg_state()
{
    const auto current_time = std::chrono::system_clock::now();

    if (!dlg_state_agregate_mode_ && (current_time - last_network_activity_time_) > dlg_state_agregate_start_timeout)
    {
        dlg_state_agregate_mode_ = true;

        agregate_start_time_ = current_time;
    }
}

void im::post_dlg_state_to_gui(const std::string& _aimid, core::icollection* _dlg_state)
{
    cached_dlg_states_.emplace_back(_aimid, _dlg_state);

    check_need_agregate_dlg_state();

    if (dlg_state_agregate_mode_)
    {
        if (dlg_state_timer_ == 0)
        {
            auto wr_this = weak_from_this();

            dlg_state_timer_ = g_core->add_timer([wr_this]
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                ptr_this->post_queued_objects_to_gui();

                const auto current_time = std::chrono::system_clock::now();

                if (current_time - ptr_this->agregate_start_time_ > dlg_state_agregate_period)
                {
                    ptr_this->dlg_state_agregate_mode_ = false;

                    ptr_this->stop_dlg_state_timer();
                }

            }, std::chrono::milliseconds(100));
        }
    }
    else
    {
        post_queued_objects_to_gui();
    }
}

void im::post_gallery_state_to_gui(const std::string& _aimid, const archive::gallery_state& _gallery_state)
{
    ifptr<icollection> cl_coll(g_core->create_collection(), true);

    _gallery_state.serialize(cl_coll.get(), _aimid);
    g_core->post_message_to_gui("dialog/gallery/state", 0, cl_coll.get());
}

void im::post_queued_objects_to_gui()
{
    post_cached_dlg_states_to_gui();
    post_queued_messages_to_gui();
}

void im::post_queued_messages_to_gui()
{
    for (const auto& _message : gui_messages_queue_)
    {
        g_core->post_message_to_gui(
            _message.get_message(),
            _message.get_seq(),
            _message.get_data().get());
    }

    gui_messages_queue_.clear();
}

void im::post_cached_dlg_states_to_gui()
{
    if (cached_dlg_states_.empty())
        return;

    //ATLTRACE("post_cached_dlg_states_to_gui %d items \r\n", cached_dlg_states_.size());

    coll_helper coll_dlgs(g_core->create_collection(), true);

    coll_dlgs.set_value_as_string("my_aimid", auth_params_->aimid_);


    ifptr<iarray> dlg_states_array(coll_dlgs->create_array());

    dlg_states_array->reserve((int32_t) cached_dlg_states_.size());

    for (const auto& _dlg_state : cached_dlg_states_)
    {
        auto _dlg_state_coll = _dlg_state.dlg_state_coll_;

        ifptr<ivalue> val(_dlg_state_coll->create_value());
        val->set_as_collection(_dlg_state_coll);
        dlg_states_array->push_back(val.get());
        _dlg_state_coll->release();

        //ATLTRACE("dlgstate %s\r\n", _dlg_state.aimid_);
    }

    cached_dlg_states_.clear();

    coll_dlgs.set_value_as_array("dlg_states", dlg_states_array.get());

    g_core->post_message_to_gui("dlg_states", 0, coll_dlgs.get());
}

void im::remove_from_cached_dlg_states(const std::string& _aimid)
{
    cached_dlg_states_.remove_if(
        [&_aimid](const auto& item) { return item.aimid_ == _aimid; }
    );
}

void im::stop_dlg_state_timer()
{
    if (dlg_state_timer_ != 0 && g_core)
    {
        g_core->stop_timer(dlg_state_timer_);

        dlg_state_timer_ = 0;
    }
}

void im::post_history_search_result_msg_to_gui(const std::string_view _contact, const archive::history_message_sptr _msg, const std::string_view _term)
{
    if (search_data_.req_id_ < 0 || !search_data_.search_results_ || _contact.empty() || !_msg)
        return;

    search::searched_message searched_msg;
    searched_msg.message_ = _msg;
    searched_msg.contact_ = _contact;
    searched_msg.highlights_.emplace_back(std::string(_term));

    search_data_.search_results_->messages_.emplace_back(std::move(searched_msg));

    if (search_data_.post_results_timer_ == empty_timer_id && g_core)
    {
        auto wr_this = weak_from_this();
        search_data_.post_results_timer_ = g_core->add_timer([wr_this, req = search_data_.req_id_]()
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this || !g_core)
                return;

            const auto stop =
                    !ptr_this->search_data_.check_req(req) ||
                    !ptr_this->search_data_.search_results_ ||
                    ptr_this->search_data_.search_results_->messages_.empty();

            if (stop)
            {
                ptr_this->search_data_.reset_post_timer();
                return;
            }

            coll_helper coll(g_core->create_collection(), true);
            ptr_this->search_data_.search_results_->serialize(coll, ptr_this->auth_params_->time_offset_);
            g_core->post_message_to_gui("dialogs/search/local/result", req, coll.get());

            ptr_this->search_data_.search_results_->messages_.clear();

        }, std::chrono::milliseconds(100));
    }
}

void im::post_history_search_result_empty()
{
    if (g_core)
    {
        g_core->post_message_to_gui("dialogs/search/local/empty", search_data_.req_id_, nullptr);
        g_core->insert_event(stats::stats_event_names::cl_search_nohistory);
    }

    search_data_.reset_post_timer();
}

std::shared_ptr<async_task_handlers> remove_messages_from_not_sent(
    const std::shared_ptr<archive::face>& _archive,
    const std::string& _contact,
    const bool _remove_if_modified,
    const std::shared_ptr<archive::history_block>& _messages)
{
    return _archive->remove_messages_from_not_sent(_contact, _remove_if_modified, _messages);
}

std::shared_ptr<async_task_handlers> remove_messages_from_not_sent(
    const std::shared_ptr<archive::face>& _archive,
    const std::string& _contact,
    const bool _remove_if_modified,
    const std::shared_ptr<archive::history_block>& _tail_messages,
    const std::shared_ptr<archive::history_block>& _intro_messages)
{
    return _archive->remove_messages_from_not_sent(_contact, _remove_if_modified, _tail_messages, _intro_messages);
}

void im::on_event_dlg_state(fetch_event_dlg_state* _event, auto_callback_sptr _on_complete)
{
    const auto &aimid = _event->get_aim_id();
    const auto &server_dlg_state = _event->get_dlg_state();
    const auto tail_messages = _event->get_tail_messages();
    const auto intro_messages = _event->get_intro_messages();

    insert_friendly(_event->get_persons(), friendly_source::remote);

    auto wr_this = weak_from_this();

    get_archive()->get_dlg_state(aimid)->on_result =
        [wr_this, aimid, _on_complete, server_dlg_state, tail_messages, intro_messages]
        (const archive::dlg_state& _local_dlg_state)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
            {
                return;
            }

            ptr_this->on_event_dlg_state_local_state_loaded(
                _on_complete,
                aimid,
                _local_dlg_state,
                server_dlg_state,
                tail_messages,
                intro_messages
            );
        };

    dlg_states_count_++;
}

void im::on_event_dlg_state_local_state_loaded(
    const auto_callback_sptr& _on_complete,
    const std::string& _aimid,
    const archive::dlg_state& _local_dlg_state,
    archive::dlg_state _server_dlg_state,
    const archive::history_block_sptr& _tail_messages,
    const archive::history_block_sptr& _intro_messages
)
{
    assert(_on_complete);
    assert(_tail_messages);
    assert(_intro_messages);
    assert(!_aimid.empty());

    const auto &dlg_patch_version = _server_dlg_state.get_dlg_state_patch_version();
    const auto &history_patch_version = _local_dlg_state.get_history_patch_version();
    const auto local_is_empty = _local_dlg_state.is_empty();
    const auto patch_version_changed = !local_is_empty && (
        !dlg_patch_version.empty() &&
        (dlg_patch_version != history_patch_version.as_string())
    );

    const auto dlg_del_up_to = _server_dlg_state.get_del_up_to();
    const auto local_del_up_to = _local_dlg_state.get_del_up_to();
    const auto del_up_to_changed = (dlg_del_up_to > local_del_up_to);
    const auto last_msg_id_changed = (!_local_dlg_state.has_last_msgid() && _server_dlg_state.has_last_msgid())
        || (_local_dlg_state.has_last_msgid() && _server_dlg_state.has_last_msgid() && (_server_dlg_state.get_last_msgid() > _local_dlg_state.get_last_msgid()));

    __INFO(
        "delete_history",
        "compared stored and incoming dialog states\n"
        "    aimid=<%1%>\n"
        "    last-msg-id=<%2%>\n"
        "    dlg-patch=  <%3%>\n"
        "    history-patch=<%4%>\n"
        "    patch-changed=<%5%>\n"
        "    local-del= <%6%>\n"
        "    server-del=<%7%>\n"
        "    del-up-to-changed=<%8%>",
        _aimid % _server_dlg_state.get_last_msgid() %
        dlg_patch_version % history_patch_version.as_string() % logutils::yn(patch_version_changed) %
        local_del_up_to % dlg_del_up_to % logutils::yn(del_up_to_changed));

    if (!_tail_messages->empty() || !_intro_messages->empty())
    {
        on_event_dlg_state_process_messages(
            *_tail_messages,
            *_intro_messages,
            _aimid,
            InOut _server_dlg_state
        );
    }
    if (_local_dlg_state.has_pinned_message() && _local_dlg_state.get_pinned_message().get_msgid() > dlg_del_up_to)
        _server_dlg_state.set_pinned_message(_local_dlg_state.get_pinned_message());

    if (_server_dlg_state.has_info_version() && _local_dlg_state.get_info_version() != _server_dlg_state.get_info_version())
    {
        get_avatar_loader()->remove_contact_avatars(_aimid, get_avatars_data_path())->on_result_ = [this, _aimid](int32_t _error)
        {
            get_contact_avatar(-1, _aimid, g_core->get_core_gui_settings().recents_avatars_size_, false);
        };
    }

    get_archive()->set_dlg_state(_aimid, _server_dlg_state)->on_result =
        [wr_this = weak_from_this(), _on_complete, _aimid, _tail_messages, _intro_messages, patch_version_changed, del_up_to_changed, last_msg_id_changed]
        (const archive::dlg_state &_local_dlg_state, const archive::dlg_state_changes&)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            ptr_this->on_event_dlg_state_local_state_updated(
                _on_complete,
                _tail_messages,
                _intro_messages,
                _aimid,
                _local_dlg_state,
                patch_version_changed,
                del_up_to_changed,
                last_msg_id_changed
            );
        };
}

void im::on_event_dlg_state_process_messages(
    const archive::history_block& _tail_messages,
    const archive::history_block& _intro_messages,
    const std::string& _aimid,
    InOut archive::dlg_state& _server_dlg_state
)
{
    assert (!_tail_messages.empty() || !_intro_messages.empty());
    for (const auto& messages : { boost::adaptors::reverse(_tail_messages), boost::adaptors::reverse(_intro_messages) })
    {
        bool found = false;
        for (const auto& msg : messages)
        {
            if (msg->get_msgid() == _server_dlg_state.get_last_msgid())
            {
                _server_dlg_state.set_last_message(*msg);
                found = true;
                break;
            }
        }
        if (found)
            break;
    }

    coll_helper coll(g_core->create_collection(), true);

    coll.set_value_as_string("aimid", _aimid);

    ifptr<iarray> senders_array(coll->create_array());
    senders_array->reserve(int32_t(_tail_messages.size() + _intro_messages.size()));
    for (const auto& messages : { boost::make_iterator_range(_tail_messages), boost::make_iterator_range(_intro_messages) })
    {
        for (const auto& message : messages)
        {
            if (!message)
                continue;

            const auto get_sender = [&message, &_aimid]() -> const std::string&
            {
                static const std::string empty;
                if (message->get_chat_data())
                    return message->get_chat_data()->get_sender();
                else if (!message->get_sender_friendly().empty())
                    return _aimid;
                return empty;
            };

            const std::string& sender = get_sender();

            if (!sender.empty())
            {
                ifptr<ivalue> val(coll->create_value());
                val->set_as_string(sender.c_str(), int32_t(sender.length()));
                senders_array->push_back(val.get());
            }
        }
    }

    coll.set_value_as_array("senders", senders_array.get());

    g_core->post_message_to_gui("messages/received/senders", 0, coll.get());

    update_active_dialogs(_aimid, _server_dlg_state);
}

void im::on_event_dlg_state_local_state_updated(
    const auto_callback_sptr& _on_complete,
    const archive::history_block_sptr& _tail_messages,
    const archive::history_block_sptr& _intro_messages,
    const std::string& _aimid,
    const archive::dlg_state& _local_dlg_state,
    const bool _patch_version_changed,
    const bool _del_up_to_changed,
    const bool _last_msg_id_changed
)
{
    const auto serialize_message = (!_tail_messages->empty() || !_intro_messages->empty() || _del_up_to_changed);
    post_dlg_state_to_gui(_aimid, false, serialize_message);

    contact_list_->update_trusted(_aimid, _local_dlg_state.get_friendly(), friendly_container_.get_nick(_aimid), !_local_dlg_state.get_suspicious());

    auto equal_id = [](auto id)
    {
        return [id](const auto& msg) { return msg->get_msgid() == id; };
    };

    if (_patch_version_changed || (_last_msg_id_changed && std::none_of(_tail_messages->cbegin(), _tail_messages->cend(), equal_id(_local_dlg_state.get_last_msgid()))))
    {
        on_history_patch_version_changed(
            _aimid,
            _local_dlg_state.get_history_patch_version(default_patch_version).as_string());
    }

    if (_del_up_to_changed && _tail_messages->empty() && _intro_messages->empty())
    {
        on_del_up_to_changed(
            _aimid,
            _local_dlg_state.get_del_up_to(),
            _on_complete);

        clear_last_message_in_dlg_state(_aimid);

        return;
    }

    remove_messages_from_not_sent(get_archive(), _aimid, false, _tail_messages, _intro_messages)->on_result_ =
        [wr_this = weak_from_this(), _on_complete, _aimid, _tail_messages, _intro_messages, _local_dlg_state, _patch_version_changed, _del_up_to_changed, _last_msg_id_changed]
        (int32_t _error)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
            {
                return;
            }

            if (ptr_this->has_opened_dialogs(_aimid))
            {
                coll_helper coll(g_core->create_collection(), true);

                coll.set_value_as_bool("result", true);
                coll.set_value_as_string("contact", _aimid);
                serialize_messages_4_gui("messages", _tail_messages, coll.get(), ptr_this->auth_params_->time_offset_);
                serialize_messages_4_gui("intro_messages", _intro_messages, coll.get(), ptr_this->auth_params_->time_offset_);
                coll.set_value_as_int64("theirs_last_delivered", _local_dlg_state.get_theirs_last_delivered());
                coll.set_value_as_int64("theirs_last_read", _local_dlg_state.get_theirs_last_read());
                coll.set<int64_t>("last_msg_in_index", _local_dlg_state.get_last_msgid());
                coll.set<std::string>("my_aimid", ptr_this->auth_params_->aimid_);

                g_core->post_message_to_gui("messages/received/dlg_state", 0, coll.get());
            }

            const int64_t last_message_id = _tail_messages->empty() ? _local_dlg_state.get_last_msgid() : _tail_messages->back()->get_msgid();

            ptr_this->get_archive()->update_history(_aimid, _tail_messages)->on_result =
                [wr_this, _aimid, _local_dlg_state, _on_complete, _patch_version_changed, _del_up_to_changed, _last_msg_id_changed, last_message_id, _tail_messages, _intro_messages]
                (archive::headers_list_sptr, const archive::dlg_state&, const archive::dlg_state_changes&, core::archive::storage::result_type _result)
                {
                    auto ptr_this = wr_this.lock();
                    if (!ptr_this)
                        return;

                    if (!_result)
                        write_files_error_in_log("update_history", _result.error_code_);

                    ptr_this->get_archive()->update_history(_aimid, _intro_messages)->on_result =
                    [wr_this, _aimid, _local_dlg_state, _on_complete, _patch_version_changed, _del_up_to_changed, _last_msg_id_changed, last_message_id, _tail_messages, _intro_messages]
                            (archive::headers_list_sptr, const archive::dlg_state&, const archive::dlg_state_changes&, core::archive::storage::result_type _result)
                    {
                        auto ptr_this = wr_this.lock();
                        if (!ptr_this)
                            return;

                        if (!_result)
                            write_files_error_in_log("update_history", _result.error_code_);

                        auto get_count = [](const archive::history_block_sptr& tail, const archive::history_block_sptr& intro)
                        {
                            const auto tail_size = int(tail->size());
                            if (intro->empty())
                                return tail_size;
                            if (tail->empty())
                                return tail_size;
                            if (tail->front()->get_prev_msgid() == intro->back()->get_msgid()) // there is no hole between intro and tail
                                return tail_size + int(intro->size());
                            return -1; // hole between intro and tail
                        };

                        ptr_this->on_event_dlg_state_history_updated(
                            _on_complete,
                            _patch_version_changed,
                            _local_dlg_state,
                            _aimid,
                            last_message_id,
                            get_count(_tail_messages, _intro_messages),
                            _del_up_to_changed,
                            _last_msg_id_changed
                        );
                    };
                };
        };
}

void im::on_event_dlg_state_history_updated(
    const auto_callback_sptr& _on_complete,
    const bool _patch_version_changed,
    const archive::dlg_state& _local_dlg_state,
    const std::string& _aimid,
    const int64_t _last_message_id,
    const int32_t _count,
    const bool _del_up_to_changed,
    const bool _last_msg_id_changed)
{
    get_archive()->has_not_sent_messages(_aimid)->on_result =
        [wr_this = weak_from_this(), _aimid, _del_up_to_changed, _local_dlg_state, _on_complete, _last_message_id, _count, _patch_version_changed, _last_msg_id_changed]
        (bool _has_not_sent_messages)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
            {
                return;
            }

            if ((_count || _patch_version_changed || _last_msg_id_changed) && _last_message_id > 0 && (_has_not_sent_messages || ptr_this->has_opened_dialogs(_aimid)))
            {
                ptr_this->download_holes(
                    _aimid,
                    _last_message_id,
                    _count > 0 ? (_count + 1) : -1);

                if (_del_up_to_changed)
                {
                    ptr_this->on_del_up_to_changed(_aimid, _local_dlg_state.get_del_up_to(), _on_complete);

                    return;
                }

                _on_complete->callback(0);

                return;
            }

            if (_del_up_to_changed)
            {
                ptr_this->on_del_up_to_changed(_aimid, _local_dlg_state.get_del_up_to(), _on_complete);
                return;
            }

            ptr_this->post_outgoing_count_to_gui(_aimid);
            _on_complete->callback(0);
        };
}

void im::on_event_hidden_chat(fetch_event_hidden_chat* _event, std::shared_ptr<auto_callback> _on_complete)
{
    if (favorites_->contains(_event->get_aimid()))
    {
        _on_complete->callback(0);
        return;
    }

    auto wr_this = weak_from_this();

    int64_t last_msg_id = _event->get_last_msg_id();
    std::string aimid = _event->get_aimid();

    get_archive()->get_dlg_state(aimid)->on_result =
        [wr_this,
        aimid,
        last_msg_id,
        _on_complete](const archive::dlg_state& _local_dlg_state)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            if (_local_dlg_state.get_last_msgid() <= last_msg_id)
            {
                archive::dlg_state updated_state = _local_dlg_state;
                updated_state.set_hidden_msg_id(last_msg_id);

                ptr_this->get_archive()->set_dlg_state(aimid, updated_state)->on_result =
                    [wr_this, aimid]
                (const archive::dlg_state&, const archive::dlg_state_changes&)
                {
                    auto ptr_this = wr_this.lock();
                    if (!ptr_this)
                        return;

                    ptr_this->active_dialogs_->remove(aimid);
                    ptr_this->remove_from_cached_dlg_states(aimid);

                    coll_helper cl_coll(g_core->create_collection(), true);
                    cl_coll.set_value_as_string("contact", aimid);
                    g_core->post_message_to_gui("active_dialogs_hide", 0, cl_coll.get());
                };

            }

            _on_complete->callback(0);
        };
}

std::wstring im::get_im_path() const
{
    int32_t id = get_id();
    if (id <= 0)
        id = default_im_id;

    char buffer[20];
    sprintf(buffer, "%04d", id);

    return tools::from_utf8(buffer);
}


void im::login_normalize_phone(int64_t _seq, const std::string& _country, const std::string& _raw_phone, const std::string& _locale, bool _is_login)
{
    if (_country.empty() || _raw_phone.empty())
    {
        assert(!"country or phone is empty");

        coll_helper cl_coll(g_core->create_collection(), true);
        cl_coll.set_value_as_bool("result", false);
        cl_coll.set_value_as_int("error", (uint32_t)  le_error_validate_phone);
        g_core->post_message_to_gui("login_get_sms_code_result", _seq, cl_coll.get());
    }

    auto packet = std::make_shared<core::wim::normalize_phone>(make_wim_params(), _country, _raw_phone);
    auto wr_this = weak_from_this();

    post_wim_packet(packet)->on_result_ = [packet, _seq, wr_this, _locale, _is_login](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (0 == _error)
        {
            phone_info _info;
            _info.set_phone(packet->get_normalized_phone());
            _info.set_locale(_locale);

            ptr_this->login_get_sms_code(_seq, _info, _is_login);
        }
        else
        {
            login_error err = le_error_validate_phone;
            switch (_error)
            {
            case wim_protocol_internal_error::wpie_network_error:
            case wim_protocol_internal_error::wpie_http_error:
                err = login_error::le_network_error;
                break;
            case wim_protocol_internal_error::wpie_invalid_phone_number:
                err = login_error::le_error_validate_phone;
            }

            coll_helper cl_coll(g_core->create_collection(), true);
            cl_coll.set_value_as_bool("result", false);
            cl_coll.set_value_as_int("error", (uint32_t) err);

            g_core->post_message_to_gui("login_get_sms_code_result", _seq, cl_coll.get());
        }
    };
}

void im::history_search_one_batch(std::shared_ptr<archive::coded_term> _cterm, std::shared_ptr<archive::contact_and_msgs> _thread_archive
                                  , std::shared_ptr<tools::binary_stream> data, int64_t _seq
                                  , int64_t _min_id)
{
    if (!search_data_.check_req(_seq))
        return;

    auto contact_and_offsets = std::make_shared<archive::contact_and_offsets_v>();

    const auto buf_size = std::min(contacts_per_search_thread, search_data_.contact_and_offsets_.size());
    contact_and_offsets->reserve(buf_size);
    for (auto index = 0u; index < buf_size; ++index)
    {
        auto item = search_data_.contact_and_offsets_.back();
        search_data_.contact_and_offsets_.pop_back();
        contact_and_offsets->push_back(item);
    }

    get_archive()->get_history_block(contact_and_offsets, _thread_archive, data)->on_result =
        [_cterm, contact_and_offsets, wr_this = weak_from_this(), _seq, _min_id]
            (std::shared_ptr<archive::contact_and_msgs> _archive, std::shared_ptr<archive::contact_and_offsets_v> _remaining, std::shared_ptr<tools::binary_stream> _data)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                if (!ptr_this->search_data_.check_req(_seq))
                    return;

                if (_remaining)
                {
                    for (const auto& item : *_remaining)
                        ptr_this->search_data_.contact_and_offsets_.push_back(item);
                }

                if (!ptr_this->history_searcher_)
                    ptr_this->history_searcher_ = std::make_unique<async_executer>("histSearch", search_threads_count);

                ptr_this->history_searcher_->run_t_async_function<search::found_messages>(
                    [_cterm, _archive, contact_and_offsets, _min_id, _data]() -> search::found_messages
                        {
                            search::found_messages found_messages;

                            if (_archive->size() > 1)
                                archive::messages_data::search_in_archive(contact_and_offsets, _cterm, _archive, _data, found_messages, _min_id);

                            return found_messages;

                        })->on_result_ = [wr_this, contact_and_offsets, _archive, _seq, _data, _cterm]
                        (search::found_messages _found_messages)
                        {
                            auto ptr_this = wr_this.lock();
                            if (!ptr_this)
                                return;

                            if (!ptr_this->search_data_.check_req(_seq))
                                return;

                            for (auto [contact, mode, offset] : *contact_and_offsets)
                            {
                                if (*mode == 0)
                                    ptr_this->search_data_.contact_and_offsets_.push_front({ contact, std::make_shared<int64_t>(2), std::make_shared<int64_t>(0) });

                                *mode = 1;

                                if (*offset > 0)
                                    ptr_this->search_data_.contact_and_offsets_.push_back({ contact, mode, offset });
                            }

                            if (!ptr_this->search_data_.contact_and_offsets_.empty())
                            {
                                int64_t last_id = -1;
                                if (ptr_this->search_data_.top_messages_.size() >= ::common::get_limit_search_results())
                                    last_id = ptr_this->search_data_.top_messages_ids_.rbegin()->first;

                                ptr_this->history_search_one_batch(_cterm, _archive, _data, ptr_this->search_data_.req_id_, last_id);
                            }
                            else
                            {
                                ++ptr_this->search_data_.free_threads_count_;
                            }

                            auto call_on_exit = std::make_shared<tools::auto_scope>([wr_this, _seq, _cterm]()
                            {
                                g_core->execute_core_context([wr_this, _seq, _cterm]()
                                {
                                    auto ptr_this = wr_this.lock();
                                    if (!ptr_this)
                                        return;

                                    if (ptr_this->search_data_.free_threads_count_ == search_threads_count
                                            || (std::chrono::system_clock::now() > ptr_this->search_data_.last_send_time_ + sending_search_results_interval))
                                    {
                                        ptr_this->search_data_.not_sent_msgs_count_ = ptr_this->search_data_.top_messages_.size();

                                        core::archive::persons_map local_persons;
                                        for (const auto& [_, val] : ptr_this->search_data_.top_messages_)
                                            get_persons_from_message(*val, local_persons);
                                        if (!local_persons.empty())
                                            ptr_this->insert_friendly(local_persons, core::friendly_source::local);

                                        for (const auto& [contact, item] : ptr_this->search_data_.top_messages_)
                                        {
                                            --ptr_this->search_data_.not_sent_msgs_count_;

                                            if (!ptr_this->search_data_.check_req(_seq)
                                                    || item->is_chat_event_deleted()
                                                    || item->is_deleted())
                                            {
                                                ptr_this->search_data_.top_messages_ids_.erase(item->get_msgid());

                                                if (ptr_this->search_data_.free_threads_count_ == search_threads_count
                                                        && ptr_this->search_data_.not_sent_msgs_count_ == 0
                                                        && ptr_this->search_data_.sent_msgs_count_ == 0)
                                                {
                                                    ptr_this->post_history_search_result_empty();
                                                }
                                            }
                                            else
                                            {
                                                ++ptr_this->search_data_.sent_msgs_count_;
                                                ptr_this->post_history_search_result_msg_to_gui(contact, item, _cterm->lower_term);
                                            }

                                            ptr_this->search_data_.top_messages_ids_[item->get_msgid()] = -1;
                                        }

                                        ptr_this->search_data_.top_messages_.clear();
                                        ptr_this->search_data_.last_send_time_ = std::chrono::system_clock::now();
                                    }

                                    if (ptr_this->search_data_.free_threads_count_ == search_threads_count && ptr_this->search_data_.top_messages_ids_.empty())
                                        ptr_this->post_history_search_result_empty();
                                });
                            });

                            for (auto& [contact, messages] : _found_messages)
                            {
                                if (messages.empty())
                                    continue;
                                std::vector<int64_t> ids;
                                ids.reserve(messages.size());
                                for (const auto& x : messages)
                                    ids.push_back(x->get_msgid());
                                ptr_this->get_archive()->filter_deleted_messages(contact, std::move(ids))->on_result = [call_on_exit, messages = std::move(messages), contact = contact, wr_this, _seq](const std::vector<int64_t>& _ids, archive::first_load _first_load) mutable
                                {
                                    auto ptr_this = wr_this.lock();
                                    if (!ptr_this)
                                        return;

                                    if (!ptr_this->has_opened_dialogs(contact))
                                        ptr_this->remove_opened_dialog(contact);
                                    else if (_first_load == archive::first_load::yes)
                                        ptr_this->get_messages_for_update(contact);

                                    if (!ptr_this->search_data_.check_req(_seq))
                                        return;
                                    auto pred = [&_ids](const auto& _msg)
                                    {
                                        return std::none_of(_ids.begin(), _ids.end(), [id = _msg->get_msgid()](auto x){ return x == id; });
                                    };
                                    messages.erase(std::remove_if(messages.begin(), messages.end(), pred), messages.end());
                                    for (const auto& msg : messages)
                                    {
                                        const auto msgid = msg->get_msgid();
                                        if (ptr_this->search_data_.top_messages_ids_.count(msgid) != 0)
                                            continue;

                                        if (ptr_this->search_data_.top_messages_ids_.size() < ::common::get_limit_search_results())
                                        {
                                            ptr_this->search_data_.top_messages_.push_back({ contact, msg });
                                            ptr_this->search_data_.top_messages_ids_.emplace(msgid, ptr_this->search_data_.top_messages_.size() - 1);
                                        }
                                        else
                                        {
                                            auto greater = ptr_this->search_data_.top_messages_ids_.upper_bound(msgid);

                                            if (greater != ptr_this->search_data_.top_messages_ids_.end())
                                            {
                                                auto index = ptr_this->search_data_.top_messages_ids_.rbegin()->second;
                                                auto min_id = ptr_this->search_data_.top_messages_ids_.rbegin()->first;

                                                if (index == -1)
                                                {
                                                    ptr_this->search_data_.top_messages_.push_back({ contact, msg });
                                                    index = ptr_this->search_data_.top_messages_.size() - 1;
                                                }
                                                else
                                                {
                                                    ptr_this->search_data_.top_messages_[index] = { contact, msg };
                                                }

                                                ptr_this->search_data_.top_messages_ids_.erase(min_id);
                                                ptr_this->search_data_.top_messages_ids_.emplace(msgid, index);
                                            }
                                        }
                                    }
                                };
                            }
                        };
            };
}

void im::setup_search_dialogs_params(int64_t _req_id)
{
    search_data_.start_time_ = std::chrono::system_clock::now();
    search_data_.last_send_time_ = std::chrono::system_clock::now() - 2 * sending_search_results_interval;
    search_data_.req_id_ = _req_id;
    search_data_.top_messages_.clear();
    search_data_.not_sent_msgs_count_ = 0;
    search_data_.sent_msgs_count_ = 0;
    search_data_.top_messages_ids_.clear();
    search_data_.contact_and_offsets_.clear();
    search_data_.free_threads_count_ = search_threads_count;

    if (_req_id != -1)
        search_data_.search_results_ = std::make_shared<search::search_dialog_results>();
    else
        search_data_.search_results_.reset();

     if (search_data_.post_results_timer_ != empty_timer_id)
         search_data_.reset_post_timer();
}

void im::clear_search_dialogs_params()
{
    setup_search_dialogs_params(-1);
}

void im::add_search_pattern_to_history(const std::string_view _search_pattern, const std::string_view _contact)
{
    get_search_history()->add_pattern(_search_pattern, _contact);
}

void im::remove_search_pattern_from_history(const std::string_view _search_pattern, const std::string_view _contact)
{
    get_search_history()->remove_pattern(_search_pattern, _contact);
}

void im::load_search_patterns(const std::string_view _contact)
{
    auto wr_this = weak_from_this();
    get_search_history()->get_patterns(_contact)->on_result = [wr_this, contact = std::string(_contact)](const core::search::search_patterns& _patterns)
    {
        if (_patterns.empty())
            return;

        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_string("contact", contact);

        ifptr<iarray> array(coll->create_array());
        array->reserve(_patterns.size());
        for (const auto& p : _patterns)
        {
            ifptr<ivalue> iv(coll->create_value());
            iv->set_as_string(p.c_str(), (int32_t)p.length());
            array->push_back(iv.get());
        }
        coll.set_value_as_array("patterns", array.get());

        g_core->post_message_to_gui("dialogs/search/pattern_history", 0, coll.get());
    };
}

void im::search_dialogs_local(const std::string& _term, const std::vector<std::string>& _aimids)
{
    if (_term.empty())
        return;

    if (search_data_.search_results_)
    {
        search_data_.search_results_->keyword_ = _term;
        search_data_.search_results_->contact_ = _aimids.empty() ? std::string() : _aimids.front();
    }

    const auto& contact_ids = _aimids.empty() ? contact_list_->get_aimids() : _aimids;
    if (contact_ids.empty())
        post_history_search_result_empty();

    for (const auto& aimid : contact_ids)
        search_data_.contact_and_offsets_.push_back({ aimid, std::make_shared<int64_t>(0), std::make_shared<int64_t>(0) });

    auto last_symb_id = std::make_shared<int32_t>(0);

    auto cterm = std::make_shared<archive::coded_term>();
    cterm->lower_term = ::tools::system::to_lower(_term);
    cterm->coded_string = tools::convert_string_to_vector(_term, last_symb_id, cterm->symbs, cterm->symb_indexes, cterm->symb_table);
    cterm->prefix = std::vector<int32_t>(tools::build_prefix(cterm->coded_string));

    const auto cof_size = search_data_.contact_and_offsets_.size();
    const auto batch_count = cof_size / contacts_per_search_thread + (cof_size % contacts_per_search_thread ? 1 : 0);
    const auto started_thread_count = std::min(search_threads_count, batch_count);
    for (size_t i = 0; i < started_thread_count; ++i)
    {
        if (search_data_.contact_and_offsets_.empty())
            break;

        auto thread_archive = std::make_shared<archive::contact_and_msgs>();

        auto data = std::make_shared<tools::binary_stream>();
        data->reserve(1024 * 1024 * 10);

        --search_data_.free_threads_count_;

        history_search_one_batch(cterm, thread_archive, data, search_data_.req_id_, -1 /* _min_id */);
    }

    if (started_thread_count == 0)
        post_history_search_result_empty();
}

void im::search_contacts_local(const std::vector<std::vector<std::string>>& _search_patterns, int64_t _seq, unsigned _fixed_patterns_count, const std::string& _pattern)
{
    cl_search_resut_v result = contact_list_->search(_search_patterns, _fixed_patterns_count, _pattern);

    coll_helper coll(g_core->create_collection(), true);
    ifptr<iarray> array(coll->create_array());
    array->reserve(result.size());
    for (const auto& res : result)
    {
        const auto& contact = res.aimid_;

        coll_helper cl_coll(g_core->create_collection(), true);
        cl_coll.set_value_as_string("contact", contact);
        cl_coll.set_value_as_bool("is_chat", contact.find("@chat.agent") != contact.npos);
        cl_coll.set_value_as_int("priority", contact_list_->get_search_priority(contact));

        if (favorites_->contains(contact))
            cl_coll.set<int64_t>("favorite_time", favorites_->get_time(contact));

        if (!res.highlights_.empty())
        {
            ifptr<iarray> hl_array(cl_coll->create_array());
            hl_array->reserve((int32_t)res.highlights_.size());
            for (const auto& hl : res.highlights_)
            {
                ifptr<ivalue> hl_value(cl_coll->create_value());
                hl_value->set_as_string(hl.c_str(), (int32_t)hl.length());
                hl_array->push_back(hl_value.get());
            }
            cl_coll.set_value_as_array("highlights", hl_array.get());
        }

        ifptr<ivalue> iv(cl_coll->create_value());
        iv->set_as_collection(cl_coll.get());
        array->push_back(iv.get());
    }
    coll.set_value_as_array("results", array.get());

    g_core->post_message_to_gui("contacts/search/local/result", _seq, coll.get());
}

void im::login_get_sms_code(int64_t _seq, const phone_info& _info, bool _is_login)
{
    auto packet = std::make_shared<core::wim::validate_phone>(make_wim_params(), _info.get_phone(), _info.get_locale());
    auto wr_this = weak_from_this();

    post_wim_packet(packet)->on_result_ = [packet, _seq, wr_this, _is_login](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == 0)
        {
            coll_helper cl_coll(g_core->create_collection(), true);
            cl_coll.set_value_as_bool("result", true);
            cl_coll.set_value_as_int("code_length", packet->get_code_length());
            cl_coll.set_value_as_string("ivr_url", packet->get_ivr_url());
            cl_coll.set_value_as_string("checks", packet->get_checks());

            auto phone_data = std::make_shared<phone_info>();
            if (_is_login)
                ptr_this->phone_registration_data_ = phone_data;
            else
                ptr_this->attached_phone_registration_data_ = phone_data;

            phone_data->set_phone(packet->get_phone());
            phone_data->set_trans_id(packet->get_trans_id());
            phone_data->set_existing(packet->get_existing());

            g_core->post_message_to_gui("login_get_sms_code_result", _seq, cl_coll.get());
        }
        else
        {
            coll_helper cl_coll(g_core->create_collection(), true);
            cl_coll.set_value_as_bool("result", false);

            login_error err = login_error::le_unknown_error;

            switch ((wim_protocol_internal_error)_error)
            {
            case wpie_network_error:
                err = login_error::le_network_error;
                break;
            case wpie_start_session_rate_limit:
                err = login_error::le_rate_limit;
                break;
            case wpie_phone_not_verified:
                err = login_error::le_invalid_sms_code;
                break;
            case wpie_error_attach_busy_phone:
                err = login_error::le_attach_error_busy_phone;
                break;
            case wpie_http_error:
            case wpie_http_empty_response:
            case wpie_http_parse_response:
            case wpie_request_timeout:
                break;
            default:
                break;
            }

            cl_coll.set_value_as_int("error", (uint32_t) err);

            g_core->post_message_to_gui("login_get_sms_code_result", _seq, cl_coll.get());
        }
    };
}

void im::login_by_phone(int64_t _seq, const phone_info& _info)
{
    if (!phone_registration_data_)
    {
        assert(!"im->phone_registration_data_ empty");
        return;
    }

    if (_info.get_sms_code().empty())
    {
        assert(!"_sms_code empty, validate it");
        return;
    }

    phone_info info_for_login(*phone_registration_data_);
    info_for_login.set_sms_code(_info.get_sms_code());

    auto packet = std::make_shared<core::wim::phone_login>(make_wim_params(), std::move(info_for_login));
    auto wr_this = weak_from_this();
    post_wim_packet(packet)->on_result_ = [packet, _seq, wr_this](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == 0)
        {
            ptr_this->auth_params_->a_token_ = packet->get_a_token();
            ptr_this->auth_params_->session_key_ = packet->get_session_key();
            ptr_this->auth_params_->exipired_in_ = packet->get_expired_in();
            ptr_this->auth_params_->time_offset_ = packet->get_time_offset();

            write_offset_in_log(ptr_this->auth_params_->time_offset_);

            ptr_this->on_login_result(_seq, 0, false);
            ptr_this->start_session(is_ping::no, is_login::yes);
        }
        else
        {
            ptr_this->on_login_result(_seq, _error, false);
        }
    };
}

void im::start_attach_phone(int64_t _seq, const phone_info& _info)
{
    if (!attached_phone_registration_data_)
    {
        assert(!"im->phone_registration_data_ empty");
        return;
    }

    if (_info.get_sms_code().empty())
    {
        assert(!"_sms_code empty, validate it");
        return;
    }

    phone_info info_for_login(*attached_phone_registration_data_);
    info_for_login.set_sms_code(_info.get_sms_code());

    auth_parameters auth_params;
    auth_params.a_token_ = auth_params_->a_token_;
    auth_params.session_key_ =  auth_params_->session_key_;

    attach_phone(_seq, auth_params, info_for_login);
}

void im::sign_url(int64_t _seq, const std::string& url)
{
    auto wim_params = make_wim_params();

    std::string signed_url;
    {
        std::stringstream ss_host;
        ss_host << std::string_view("https://www.icq.com/karma_api/karma_client2web_login.php");

        std::map<std::string, std::string> params;

        params["ts"] = tools::from_int64(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) - wim_params.time_offset_);
        params["owner"] = wim_params.aimid_;

        params["a"] = wim_packet::escape_symbols(wim_params.a_token_);
        params["k"] = wim_params.dev_id_;
        params["d"] = wim_packet::escape_symbols(url);
        std::string hash_data = ("GET&" + wim_packet::escape_symbols(ss_host.str()) + '&' + wim_packet::escape_symbols(wim_packet::create_query_from_map(params)));
        std::string sha256 = wim_packet::detect_digest(hash_data, wim_params.session_key_);
        params["sig_sha256"] = wim_packet::escape_symbols(sha256);

        //params["a"] = es(params["a"]);
        //params["k"] = es(params["k"]);
        //params["d"] = es(params["d"]);
        //params["sig_sha256"] = es(params["sig_sha256"]);

        std::stringstream ss_url;
        ss_url << ss_host.str() << '?' << wim_packet::format_get_params(params);
        signed_url = ss_url.str();
    }

    auto icoll = g_core->create_collection();
    coll_helper coll(icoll, true);
    coll.set_value_as_string("url", signed_url);

    g_core->post_message_to_gui("signed_url", _seq, coll.get());
}

void im::resume_failed_network_requests()
{
    auto wr_this = weak_from_this();

    async_tasks_->run_async_function([]()->int32_t
    {
        return 0;

    })->on_result_ = [wr_this](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->post_pending_messages();
        ptr_this->post_pending_delete_messages();
        ptr_this->resume_failed_dlg_states();
        ptr_this->resume_all_file_sharing_uploading();
        ptr_this->resume_failed_avatars();
        ptr_this->download_failed_holes();
        ptr_this->resume_failed_update_messages();
        ptr_this->resume_stickers_migration();
        ptr_this->resume_download_stickers();
        ptr_this->resume_download_masks();
        ptr_this->resume_failed_user_agreements();
        ptr_this->resume_failed_set_attention();

        ptr_this->get_async_loader().resume_suspended_tasks(ptr_this->make_wim_params());
    };
}

void im::resume_failed_avatars()
{
    get_avatar_loader()->resume(make_wim_params());
}

void im::send_user_agreement(const accept_agreement_info &_info, int64_t _seq)
{
    // https://jira.mail.ru/browse/IMDESKTOP-7771
    if (suppress_gdpr_resend_.load()
            && !_info.reset_)
        return;

    auto wr_this = weak_from_this();
    auto packet = std::make_shared<core::wim::gdpr_agreement>(make_wim_params(), _info);

    auto on_result = [wr_this, _seq, info = _info](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error != 0) //        if (_error == wpie_network_error)
        {
            std::cerr << "Error " << _error << " , " << (_error == wpie_error_request_canceled_wait_timeout)
                      << std::endl;
            ptr_this->set_resend_gdpr_info(std::make_unique<accept_agreement_info>(info));
        }

        core::configuration::app_config::gdpr_report_to_server_state state = (_error != 0 ? core::configuration::app_config::gdpr_report_to_server_state::failed
            : core::configuration::app_config::gdpr_report_to_server_state::reported_successfully);

        core::configuration::set_config_option(core::configuration::app_config::AppConfigOption::gdpr_agreement_reported_to_server,
            static_cast<int32_t>(state));

        const auto app_ini_path = utils::get_app_ini_path();
        configuration::dump_app_config_to_disk(app_ini_path);

        // Report new config to GUI
        core::coll_helper cl_coll(g_core->create_collection(), true);
        configuration::get_app_config().serialize(Out cl_coll);
        g_core->post_message_to_gui("app_config", 0, cl_coll.get());

        // report back to gui
        if (_seq)
            g_core->post_message_to_gui("user_accept_gdpr_result", _seq, nullptr);
    };

    post_wim_packet(packet)->on_result_ = on_result;
}

void im::resume_failed_user_agreements()
{
    // add logic to constructor to load from configuration
    if (!resend_gdpr_info_)
        return;

    auto info = *resend_gdpr_info_;
    set_resend_gdpr_info(nullptr);

    send_user_agreement(info);
}

void im::resume_failed_dlg_states()
{
    if (failed_dlg_states_.empty())
        return;

    auto wr_this = weak_from_this();

    for (const auto& _aimid : failed_dlg_states_)
    {
        get_archive()->get_dlg_state(_aimid)->on_result = [wr_this, _aimid](const archive::dlg_state& _dlg_state)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            const auto last_read_text = _dlg_state.get_yours_last_read();
            const auto last_read_mention = _dlg_state.get_last_read_mention();

            if (last_read_text > 0 && last_read_text == last_read_mention)
            {
                set_dlg_state_params params;
                params.aimid_ = _aimid;
                params.last_read_ = last_read_text;
                ptr_this->set_dlg_state(std::move(params));
            }
            else
            {
                if (last_read_mention > 0)
                {
                    set_dlg_state_params params;
                    params.aimid_ = _aimid;
                    params.last_read_ = last_read_mention;
                    params.set_exclude(set_dlg_state_params::exclude::text);
                    ptr_this->set_dlg_state(std::move(params));
                }
                if (last_read_text > 0)
                {
                    set_dlg_state_params params;
                    params.aimid_ = _aimid;
                    params.last_read_ = last_read_text;
                    params.set_exclude(set_dlg_state_params::exclude::mention);
                    ptr_this->set_dlg_state(std::move(params));
                }
            }
        };
    }

    failed_dlg_states_.clear();
}


void im::resume_failed_set_attention()
{
    if (failed_set_attention_.empty())
        return;

    for (const auto& _aimid : failed_set_attention_)
    {
        get_archive()->get_dlg_state(_aimid)->on_result = [wr_this = weak_from_this(), _aimid](const archive::dlg_state& _dlg_state)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            ptr_this->set_attention_attribute(0, _aimid, _dlg_state.get_attention(), true);
        };
    }


    failed_set_attention_.clear();
}

void im::resume_failed_update_messages()
{
    if (failed_update_messages_.empty())
        return;

    for (const auto& _aimid : failed_update_messages_)
        get_messages_for_update(_aimid);

    failed_update_messages_.clear();
}


void im::get_contact_avatar(int64_t _seq, const std::string& _contact, int32_t _avatar_size, bool _force)
{
    auto context = std::make_shared<avatar_context>(_avatar_size, _contact, get_avatars_data_path());
    context->force_ = _force;

    auto handler = get_avatar_loader()->get_contact_avatar_async(make_wim_params(), context);

    handler->completed_ = handler->updated_ = [wr_this = weak_from_this(), _seq](std::shared_ptr<avatar_context> _context)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        coll_helper coll(g_core->create_collection(), true);
        ifptr<istream> avatar_stream(coll->create_stream());
        uint32_t size = _context->avatar_data_.available();
        avatar_stream->write((uint8_t*)_context->avatar_data_.read(size), size);
        coll.set_value_as_stream("avatar", avatar_stream.get());

        coll.set_value_as_bool("result", true);
        coll.set_value_as_string("contact", _context->contact_);
        coll.set_value_as_int("size", _context->avatar_size_);

        _context->avatar_data_.reset();

        const bool avatar_need_to_convert = ptr_this->on_voip_avatar_actual_for_voip(_context->contact_, _context->avatar_size_);

        coll.set_value_as_bool("avatar_need_to_convert", avatar_need_to_convert);

        g_core->post_message_to_gui("avatars/get/result", _seq, coll.get());
    };

    handler->failed_ = [_seq](std::shared_ptr<avatar_context> _context, int32_t _error)
    {
        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_bool("result", false);
        coll.set_value_as_string("contact", _context->contact_);
        coll.set_value_as_int("size", _context->avatar_size_);

        avatar_error av_err = avatar_error::ae_unknown_error;
        if (_error == wim_protocol_internal_error::wpie_network_error)
            av_err = avatar_error::ae_network_error;

        coll.set_value_as_int("error", av_err);
        coll.set_value_as_bool("avatar_need_to_convert", false);

        g_core->post_message_to_gui("avatars/get/result", _seq, coll.get());
    };
}

void im::show_contact_avatar(int64_t _seq, const std::string& _contact, int32_t _avatar_size)
{
    get_avatar_loader()->show_contact_avatar(_contact, _avatar_size);
}

void im::remove_contact_avatars(int64_t _seq, const std::string& _contact)
{
    get_avatar_loader()->remove_contact_avatars(_contact, get_avatars_data_path());
}

std::shared_ptr<avatar_loader> im::get_avatar_loader()
{
    if (!avatar_loader_)
        avatar_loader_ = std::make_shared<avatar_loader>();

    return avatar_loader_;
}

std::shared_ptr<send_message_handler> im::send_message_async(
    int64_t _seq,
    const int64_t _updated_id,
    const std::string& _contact,
    const std::string& _message,
    const std::string& _internal_id,
    const core::message_type _type,
    const core::archive::quotes_vec& _quotes,
    const core::archive::mentions_map& _mentions,
    const std::string& _description,
    const std::string& _url,
    const core::archive::shared_contact& _shared_contact)
{
    assert(_type > message_type::min);
    assert(_type < message_type::max);

    auto handler = std::make_shared<send_message_handler>();

    auto packet = std::make_shared<core::wim::send_message>(make_wim_params(), _updated_id, _type, _internal_id, _contact, _message, _quotes, _mentions, _description, _url, _shared_contact);

    post_wim_packet(packet)->on_result_ = [handler, packet, wr_this = weak_from_this(), _contact](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (handler->on_result)
            handler->on_result(_error, *packet);
    };

    return handler;
}

static archive::not_sent_message_sptr make_message(const std::string& _contact, core::archive::message_pack&& _pack, const std::shared_ptr<auth_parameters>& _auth_params)
{
    assert(_pack.type_ > message_type::min);
    assert(_pack.type_ < message_type::max);


    uint64_t time;
    if (_pack.message_time_ != 0)
        time = _pack.message_time_;
    else
    {
        time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        time -= _auth_params->time_offset_;
    }

    for (auto& q : _pack.quotes_)
    {
        int32_t quote_time = q.get_time();
        quote_time -= int32_t(_auth_params->time_offset_);
        q.set_time(quote_time);
    }

    archive::not_sent_message_sptr msg_not_sent;

    if (_pack.type_ == core::message_type::file_sharing)
    {
        msg_not_sent = archive::not_sent_message::make_incoming_file_sharing(_contact, time, std::move(_pack.message_), std::move(_pack.internal_id_));
    }
    else
    {
        msg_not_sent = archive::not_sent_message::make(_contact, std::move(_pack.message_), _pack.type_, time, std::move(_pack.internal_id_));
        msg_not_sent->set_calc_stat(true);
    }

    msg_not_sent->set_description(std::move(_pack.description_));
    msg_not_sent->set_url(std::move(_pack.url_));
    msg_not_sent->attach_quotes(std::move(_pack.quotes_));
    msg_not_sent->set_mentions(std::move(_pack.mentions_));

    if (_pack.shared_contact_)
        msg_not_sent->set_shared_contact(_pack.shared_contact_);

    auto hist_msg = msg_not_sent->get_message();
    hist_msg->set_update_patch_version(std::move(_pack.version_));
    if (_contact.find("@chat.agent") != _contact.npos)
    {
        if (!hist_msg->get_chat_data())
            hist_msg->set_chat_data(archive::chat_data());
        hist_msg->get_chat_data()->set_sender(_auth_params->aimid_);
    }

    return msg_not_sent;
}

static archive::not_sent_message_sptr make_message(const std::string& _contact, int64_t updated_id, core::archive::message_pack&& _pack, const std::shared_ptr<auth_parameters>& _auth_params)
{
    auto res = make_message(_contact, std::move(_pack), _auth_params);
    res->set_updated_id(updated_id);
    return res;
}

void im::send_message_to_contact(
    int64_t _seq,
    const std::string& _contact,
    core::archive::message_pack _pack)
{
    auto msg_not_sent = make_message(_contact, std::move(_pack), auth_params_);

    get_archive()->insert_not_sent_message(_contact, msg_not_sent);

    if (has_opened_dialogs(_contact))
        post_not_sent_message_to_gui(_seq, msg_not_sent);

    post_pending_messages();

    get_archive()->get_dlg_state(_contact)->on_result = [wr_this = weak_from_this(), _contact, msg_not_sent] (const archive::dlg_state& _local_dlg_state)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        archive::dlg_state new_dlg_state = _local_dlg_state;

        auto message = msg_not_sent->get_message();

        if (message->has_msgid())
            new_dlg_state.set_last_msgid(message->get_msgid());

        new_dlg_state.set_last_message(*message);
        new_dlg_state.set_unread_count(0);
        new_dlg_state.set_visible(true);
        new_dlg_state.set_fake(true);

        ptr_this->get_archive()->set_dlg_state(_contact, new_dlg_state)->on_result =
            [wr_this, _contact]
            (const archive::dlg_state&, const archive::dlg_state_changes&)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                ptr_this->post_dlg_state_to_gui(_contact);
            };
    };
}

void im::send_message_to_contacts(
    int64_t _seq,
    const std::vector<std::string>& _contacts,
    core::archive::message_pack _pack)
{
    for (const auto& contact : _contacts)
        send_message_to_contact(_seq, contact, _pack);
}

void im::update_message_to_contact(
    const int64_t _seq,
    const int64_t _id,
    const std::string& _contact,
    core::archive::message_pack _pack)
{
    auto msg_not_sent = make_message(_contact, _id, std::move(_pack), auth_params_);

    msg_not_sent->get_message()->increment_offline_version();

    if (_id == -1)
    {
        msg_not_sent->set_modified(true);

        if (!get_archive()->update_if_exist_not_sent_message(_contact, msg_not_sent))
        {
        }
    }
    else
    {
        get_archive()->update_message_data(_contact, *msg_not_sent->get_message());

        get_archive()->insert_not_sent_message(_contact, msg_not_sent);
    }

    if (has_opened_dialogs(_contact))
        post_not_sent_message_to_gui(_seq, msg_not_sent);


    if (_id != -1)
    {
        coll_helper coll(g_core->create_collection(), true);
        coll.set<bool>("result", true);
        coll.set<std::string>("contact", _contact);
        coll.set<std::string>("my_aimid", auth_params_->aimid_);

        auto messages = std::make_shared<archive::history_block>();

        messages->emplace_back(archive::history_message::make_updated(_id, msg_not_sent->get_internal_id()));

        serialize_message_ids_4_gui("ids", messages, coll.get(), auth_params_->time_offset_, from_deleted_mode::no, skip_patches::no);

        g_core->post_message_to_gui("messages/received/updated", 0, coll.get());
    }

    if (!msg_not_sent->get_url().empty() || msg_not_sent->get_description().empty())
    {
        post_pending_messages();
    }

    get_archive()->get_dlg_state(_contact)->on_result = [wr_this = weak_from_this(), _contact, msg_not_sent](const archive::dlg_state& _local_dlg_state)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        auto message = msg_not_sent->get_message();
        if (message->has_msgid() && _local_dlg_state.has_last_msgid() && message->get_msgid() == _local_dlg_state.get_last_msgid())
        {
            archive::dlg_state new_dlg_state = _local_dlg_state;
            new_dlg_state.set_last_message(*message);
            new_dlg_state.set_unread_count(0);
            new_dlg_state.set_visible(true);
            new_dlg_state.set_fake(true);

            ptr_this->get_archive()->set_dlg_state(_contact, new_dlg_state)->on_result =
                [wr_this, _contact]
            (const archive::dlg_state&, const archive::dlg_state_changes&)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                ptr_this->post_dlg_state_to_gui(_contact);
            };
        }
    };
}

void im::send_message_typing(int64_t _seq, const std::string& _contact, const core::typing_status& _status, const std::string& _id)
{
    auto packet = std::make_shared<core::wim::send_message_typing>(make_wim_params(), _contact, _status, _id);

    post_wim_packet(packet)->on_result_ = [wr_this = weak_from_this()](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->set_connection_state(_error == wim_protocol_internal_error::wpie_network_error ? connection_state::connecting : connection_state::online);
    };
}

void im::send_feedback(const int64_t _seq, const std::string &url, const std::map<std::string, std::string> &fields, const std::vector<std::string> &files)
{
    auto packet = std::make_shared<core::wim::send_feedback>(make_wim_params(), url, fields, files);

    post_wim_packet(packet)->on_result_ = [_seq](int32_t _error)
    {
        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_bool("succeeded", (_error == 0));
        g_core->post_message_to_gui("feedback/sent", _seq, coll.get());

        if (_error == 0)
            g_core->insert_event(stats::stats_event_names::feedback_sent);
        else
            g_core->insert_event(stats::stats_event_names::feedback_error);
    };;
}

void im::set_state(const int64_t _seq, const core::profile_state _state)
{
    auto packet = std::make_shared<core::wim::set_state>(make_wim_params(), _state);

    post_wim_packet(packet);
}

void im::phoneinfo(int64_t seq, const std::string &phone, const std::string &gui_locale)
{
    auto packet = std::make_shared<core::wim::phoneinfo>(make_wim_params(), phone, gui_locale);
    post_wim_packet(packet)->on_result_ = [seq, packet](int32_t _error)
    {
        if (_error == 0)
        {
            coll_helper coll(g_core->create_collection(), true);
            coll.set_value_as_string("operator", packet->info_operator_);
            coll.set_value_as_string("phone", packet->info_phone_);
            coll.set_value_as_string("iso_country", packet->info_iso_country_);
            if (!packet->printable_.empty())
            {
                ifptr<iarray> array(coll->create_array());
                array->reserve((int32_t)packet->printable_.size());
                for (const auto& v : packet->printable_)
                {
                    ifptr<ivalue> iv(coll->create_value());
                    iv->set_as_string(v.c_str(), (int32_t)v.length());
                    array->push_back(iv.get());
                }
                coll.set_value_as_array("printable", array.get());
            }
            coll.set_value_as_string("status", packet->status_);
            coll.set_value_as_bool("is_phone_valid", packet->is_phone_valid_);
            coll.set_value_as_string("trunk_code", packet->trunk_code_);
            coll.set_value_as_string("modified_phone_number", packet->modified_phone_number_);
            if (!packet->remaining_lengths_.empty())
            {
                ifptr<iarray> array(coll->create_array());
                array->reserve((int32_t)packet->remaining_lengths_.size());
                for (const auto v : packet->remaining_lengths_)
                {
                    ifptr<ivalue> iv(coll->create_value());
                    iv->set_as_int64(v);
                    array->push_back(iv.get());
                }
                coll.set_value_as_array("remaining_lengths", array.get());
            }
            if (!packet->prefix_state_.empty())
            {
                ifptr<iarray> array(coll->create_array());
                array->reserve((int32_t)packet->prefix_state_.size());
                for (const auto& v : packet->prefix_state_)
                {
                    ifptr<ivalue> iv(coll->create_value());
                    iv->set_as_string(v.c_str(), (int32_t)v.length());
                    array->push_back(iv.get());
                }
                coll.set_value_as_array("prefix_state", array.get());
            }
            coll.set_value_as_string("modified_prefix", packet->modified_prefix_);
            g_core->post_message_to_gui("phoneinfo/result", seq, coll.get());
        }
    };;
}

void im::remove_members(int64_t _seq, const std::string& _aimid, const std::string& _m_chat_members)
{
    auto packet = std::make_shared<core::wim::remove_members>(make_wim_params(), _aimid, _m_chat_members);

    //active_dialogs_->remove(_aimid);

    post_wim_packet(packet);
}


void im::add_members(int64_t _seq, const std::string& _aimid, const std::string& _m_chat_members_to_add)
{
    auto packet = std::make_shared<core::wim::add_members>(make_wim_params(), _aimid, _m_chat_members_to_add);
    auto wr_this = weak_from_this();

    //active_dialogs_->remove(_aimid);

    //if (contact_list_->exist(_aimid))
    post_wim_packet(packet);
}


void im::add_chat(int64_t _seq, const std::string& _m_chat_name, std::vector<std::string> _m_chat_members)
{
    auto packet = std::make_shared<core::wim::add_chat>(make_wim_params(), _m_chat_name, std::move(_m_chat_members));
    post_wim_packet(packet);
}

void im::get_mrim_key(int64_t _seq, const std::string& _email)
{
    auto packet = std::make_shared<core::wim::mrim_get_key>(make_wim_params(), _email);

    post_wim_packet(packet)->on_result_ = [packet, _seq, wr_this = weak_from_this()](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;
        auto key = packet->get_mrim_key();
        if (_error == 0 && !key.empty())
        {
            coll_helper coll(g_core->create_collection(), true);
            coll.set_value_as_string("key", key);
            g_core->post_message_to_gui("mrim/get_key/result", _seq, coll.get());
        }
    };

}

void im::on_history_patch_version_changed(const std::string& _aimid, const std::string& _history_patch_version)
{
    assert(!_aimid.empty());
    assert(!_history_patch_version.empty());

    const get_history_params params(
        _aimid,
        -1,
        -1,
        -1,
        _history_patch_version,
        false,
        true);

    __INFO(
        "delete_history",
        "history patch version changed, download history anew\n"
        "    contact=<%1%>\n"
        "    request-from=<%2%>\n"
        "    patch=<%3%>",
        _aimid % -1 % _history_patch_version);

    get_history_from_server(params);
}

void im::on_del_up_to_changed(const std::string& _aimid, const int64_t _del_up_to, const auto_callback_sptr& _on_complete)
{
    assert(!_aimid.empty());
    assert(_del_up_to > -1);
    assert(_on_complete);

    __INFO(
        "delete_history",
        "requesting history deletion\n"
        "    contact=<%1%>\n"
        "    up-to=<%2%>",
        _aimid % _del_up_to
    );

    get_archive()->delete_messages_up_to(_aimid, _del_up_to)->on_result_ =
        [_on_complete, _aimid, _del_up_to](int32_t error)
        {
            coll_helper cl_coll(g_core->create_collection(), true);
            cl_coll.set<std::string>("contact", _aimid);
            cl_coll.set<int64_t>("id", _del_up_to);

            g_core->post_message_to_gui("messages/del_up_to", 0, cl_coll.get());

            _on_complete->callback(error);
        };
}

void im::modify_chat(int64_t _seq, const std::string& _aimid, const std::string& _m_chat_name)
{
    auto packet = std::make_shared<core::wim::modify_chat>(make_wim_params(), _aimid, _m_chat_name);

    //active_dialogs_->remove(_aimid);

    //if (contact_list_->exist(_aimid))
    post_wim_packet(packet)->on_result_ = [](int32_t _error)
    {
        if (_error == 0)
            g_core->insert_event(stats::stats_event_names::chats_rename);
    };
}


std::shared_ptr<async_task_handlers> im::send_pending_message_async(int64_t _seq, const archive::not_sent_message_sptr& _message)
{
    auto handler = std::make_shared<async_task_handlers>();

    const auto &pending_message = _message->get_message();

    std::stringstream log;
    log << "CORE: send pending async\r\n";
    log << "for: " << _message->get_aimid() << "\r\n";
    log << "reqId: " << pending_message->get_internal_id() << "\r\n";
    g_core->write_string_to_network_log(log.str());

    send_message_async(
        _seq,
        pending_message->get_msgid(),
        _message->get_aimid(),
        pending_message->get_text(),
        _message->get_network_request_id(),
        pending_message->get_type(),
        _message->get_quotes(),
        _message->get_mentions(),
        _message->get_description(),
        _message->get_url(),
        _message->get_shared_contact())->on_result = [wr_this = weak_from_this(), handler, _message](int32_t _error, const send_message& _packet)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        const auto auto_handler = std::make_shared<tools::auto_scope>([handler, _error]
        {
            if (handler->on_result_)
                handler->on_result_(_error);
        });

        if (_error == 0)
        {
            uint64_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) - ptr_this->auth_params_->time_offset_;
            auto delay = time - _message->get_message()->get_time();

            if (delay > core::stats::msg_pending_delay_s)
            {
                core::stats::event_props_type props;
                props.emplace_back("delay", std::to_string(delay));
                g_core->insert_event(core::stats::stats_event_names::message_pending, props);
            }

            if (_packet.get_hist_msg_id() > 0)
            {
                const auto msg = std::make_shared<archive::history_message>(*(_message->get_message())); // make deep copy to avoid race condition since _message is shallow copy of message at pending map

                auto messages_block = std::make_shared<archive::history_block>();
                msg->set_msgid(_packet.get_hist_msg_id());
                msg->set_prev_msgid(_packet.get_before_hist_msg_id());

                messages_block->push_back(msg);

                ptr_this->get_archive()->update_history(_message->get_aimid(), messages_block)->on_result = [wr_this, _message, messages_block, auto_handler](
                    std::shared_ptr<archive::headers_list> _inserted_messages,
                    const archive::dlg_state&,
                    const archive::dlg_state_changes& _changes_on_update,
                    core::archive::storage::result_type _result)
                {
                    auto ptr_this = wr_this.lock();
                    if (!ptr_this)
                        return;

                    coll_helper coll(g_core->create_collection(), true);

                    coll.set_value_as_bool("result", true);
                    coll.set_value_as_string("contact", _message->get_aimid());
                    coll.set<std::string>("my_aimid", ptr_this->auth_params_->aimid_);
                    serialize_messages_4_gui("messages", messages_block, coll.get(), ptr_this->auth_params_->time_offset_);

                    g_core->post_message_to_gui("messages/received/message_status", 0, coll.get());

                    if (!_result)
                        write_files_error_in_log("update_history", _result.error_code_);

                    ptr_this->get_archive()->remove_message_from_not_sent(_message->get_aimid(), false, messages_block->front())->on_result_ = [wr_this, _message, auto_handler](int32_t result)
                    {
                        auto ptr_this = wr_this.lock();
                        if (!ptr_this)
                            return;
                        ptr_this->post_outgoing_count_to_gui(_message->get_aimid());
                    };
                };
            }
            else
            {
                auto message = archive::not_sent_message::make(_message, _packet.get_wim_msg_id(), time);

                ptr_this->get_archive()->update_if_exist_not_sent_message(_message->get_aimid(), message);
            }
        }
        else if (_packet.get_http_code() == 200)
        {
            switch (_packet.get_status_code())
            {
            case wim_protocol_error::INVALID_REQUEST:
            {
                if (_packet.get_status_detail_code() == wim_protocol_subcodes::no_such_session)
                    break;
            }
            case wim_protocol_error::PARAMETER_ERROR:
            case wim_protocol_error::INVALID_TARGET:
            case wim_protocol_error::TARGET_DOESNT_EXIST:
            case wim_protocol_error::TARGET_NOT_AVAILABLE:
            case wim_protocol_error::MESSAGE_TOO_BIG:
            case wim_protocol_error::CAPCHA:
            case wim_protocol_error::MISSING_REQUIRED_PARAMETER:
                ptr_this->get_archive()->remove_message_from_not_sent(_message->get_aimid(), true, _message->get_message());
                break;
            case wim_protocol_error::MORE_AUTHN_REQUIRED:
            case wim_protocol_error::AUTHN_REQUIRED:
            case wim_protocol_error::SESSION_NOT_FOUND:    //Migration in process
            case wim_protocol_error::REQUEST_TIMEOUT:
                break;
            default:
                break;
            }
        }
    };

    return handler;
}

void im::post_not_sent_message_to_gui(int64_t _seq, const archive::not_sent_message_sptr& _message)
{
    coll_helper cl_coll(g_core->create_collection(), true);
    cl_coll.set_value_as_string("contact", _message->get_aimid());
    ifptr<iarray> messages_array(cl_coll->create_array());
    messages_array->reserve(1);
    coll_helper coll_message(cl_coll->create_collection(), true);
    _message->serialize(coll_message, auth_params_->time_offset_);
    ifptr<ivalue> val(cl_coll->create_value());
    val->set_as_collection(coll_message.get());
    messages_array->push_back(val.get());
    cl_coll.set_value_as_array("messages", messages_array.get());
    cl_coll.set<std::string>("my_aimid", auth_params_->aimid_);
    cl_coll.set_value_as_bool("result", true);
    g_core->post_message_to_gui("archive/messages/pending", _seq, cl_coll.get());
}

void im::add_opened_dialog(const std::string& _contact)
{
    const auto& [_, added] = opened_dialogs_.emplace(_contact, archive::opened_dialog());
    if (added)
        load_search_patterns(_contact);
}

void im::remove_opened_dialog(const std::string& _contact)
{
    opened_dialogs_.erase(_contact);

    get_archive()->free_dialog(_contact);
    remove_postponed_for_update_dialog(_contact);
}

bool im::has_opened_dialogs(const std::string& _contact) const
{
    return opened_dialogs_.find(_contact) != opened_dialogs_.end();
}

void im::set_last_read(const std::string& _contact, int64_t _message, message_read_mode _mode)
{
    get_archive()->get_dlg_state(_contact)->on_result = [wr_this = weak_from_this(), _contact, _message, _mode](const archive::dlg_state& _local_dlg_state)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_local_dlg_state.get_fake())
            return;

        const bool read_all = _mode == message_read_mode::read_all;

        if (_local_dlg_state.get_yours_last_read() < _message || _local_dlg_state.get_unread_count() != 0 || (read_all && _local_dlg_state.get_last_read_mention() < _message))
        {
            archive::dlg_state new_dlg_state = _local_dlg_state;
            new_dlg_state.set_yours_last_read(_message);
            new_dlg_state.set_unread_count(0);

            if (read_all)
                new_dlg_state.set_last_read_mention(_message);

            ptr_this->get_archive()->set_dlg_state(_contact, new_dlg_state)->on_result =
                [wr_this, _contact, read_all]
            (const archive::dlg_state &_local_dlg_state, const archive::dlg_state_changes&)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                set_dlg_state_params params;
                params.aimid_ = _contact;
                params.last_read_ = _local_dlg_state.get_yours_last_read();
                if (!read_all)
                    params.set_exclude(set_dlg_state_params::exclude::mention);
                ptr_this->set_dlg_state(std::move(params))->on_result_ = [wr_this, _contact] (int32_t error)
                {
                    auto ptr_this = wr_this.lock();
                    if (!ptr_this)
                        return;

                    ptr_this->post_dlg_state_to_gui(_contact);
                };
            };
        }
    };
}

void im::set_last_read_mention(const std::string& _contact, int64_t _message)
{
    get_archive()->get_dlg_state(_contact)->on_result = [wr_this = weak_from_this(), _contact, _message](const archive::dlg_state& _local_dlg_state)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_local_dlg_state.get_fake())
            return;

        if (_local_dlg_state.get_last_read_mention() < _message)
        {
            archive::dlg_state new_dlg_state = _local_dlg_state;
            new_dlg_state.set_last_read_mention(_message);
            new_dlg_state.set_unread_mentions_count(std::nullopt);
            new_dlg_state.set_unread_count(std::nullopt);

            ptr_this->get_archive()->set_dlg_state(_contact, new_dlg_state)->on_result =
                [wr_this, _contact]
            (const archive::dlg_state &_local_dlg_state, const archive::dlg_state_changes&)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                set_dlg_state_params params;
                params.aimid_ = _contact;
                params.last_read_ = _local_dlg_state.get_last_read_mention();
                params.set_exclude(set_dlg_state_params::exclude::text);
                ptr_this->set_dlg_state(std::move(params))->on_result_ = [wr_this, _contact](int32_t error)
                {
                    auto ptr_this = wr_this.lock();
                    if (!ptr_this)
                        return;

                    ptr_this->post_dlg_state_to_gui(_contact);
                };
            };
        }
    };
}

void im::set_last_read_partial(const std::string& _contact, int64_t _message)
{
    get_archive()->get_dlg_state(_contact)->on_result = [wr_this = weak_from_this(), _contact, _message](const archive::dlg_state& _local_dlg_state)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_local_dlg_state.get_fake())
            return;

        if (_local_dlg_state.get_last_read_mention() < _message
            || _local_dlg_state.get_yours_last_read() < _message
            || (_local_dlg_state.get_unread_count() != 0 && _local_dlg_state.get_yours_last_read() == _message))
        {
            archive::dlg_state new_dlg_state = _local_dlg_state;
            new_dlg_state.set_last_read_mention(_message);
            new_dlg_state.set_yours_last_read(_message);
            new_dlg_state.set_unread_count(std::nullopt);
            new_dlg_state.set_unread_mentions_count(std::nullopt);

            ptr_this->get_archive()->set_dlg_state(_contact, new_dlg_state)->on_result =
                [wr_this, _contact]
            (const archive::dlg_state &_local_dlg_state, const archive::dlg_state_changes&)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                set_dlg_state_params params;
                params.aimid_ = _contact;
                params.last_read_ = _local_dlg_state.get_yours_last_read();
                ptr_this->set_dlg_state(std::move(params))->on_result_ = [wr_this, _contact](int32_t error)
                {
                    auto ptr_this = wr_this.lock();
                    if (!ptr_this)
                        return;

                    ptr_this->post_dlg_state_to_gui(_contact);
                };
            };
        }
    };
}

void im::hide_dlg_state(const std::string& _contact)
{
    get_archive()->get_dlg_state(_contact)->on_result = [wr_this = weak_from_this(), _contact](const archive::dlg_state& _local_dlg_state)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        archive::dlg_state new_dlg_state = _local_dlg_state;

        new_dlg_state.set_visible(false);
        new_dlg_state.set_unread_count(std::nullopt);

        ptr_this->get_archive()->set_dlg_state(_contact, new_dlg_state);
    };
}

void im::clear_last_message_in_dlg_state(const std::string& _contact)
{
    get_archive()->get_dlg_state(_contact)->on_result = [wr_this = weak_from_this(), _contact](const archive::dlg_state& _local_dlg_state)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        archive::dlg_state new_dlg_state = _local_dlg_state;
        auto message = new_dlg_state.get_last_message();
        message.merge(archive::history_message());
        new_dlg_state.set_last_message(message);
        new_dlg_state.set_unread_count(0);
        new_dlg_state.set_attention(false);
        new_dlg_state.set_visible(true);
        new_dlg_state.set_fake(true);

        ptr_this->get_archive()->set_dlg_state(_contact, new_dlg_state)->on_result =
            [wr_this, _contact]
        (const archive::dlg_state&, const archive::dlg_state_changes&)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            ptr_this->post_dlg_state_to_gui(_contact, false, true, true, true);
        };
    };
}

void im::delete_archive_message_local(const int64_t _seq, const std::string &_contact_aimid, const int64_t _message_id, const std::string& _internal_id, const bool _for_all)
{
    auto messages = std::make_shared<archive::history_block>();

    if (_for_all)
    {
        messages->emplace_back(archive::history_message::make_modified_patch(_message_id, _internal_id));
    }
    else
    {
        messages->emplace_back(archive::history_message::make_deleted_patch(_message_id, _internal_id));
    }

    coll_helper coll(g_core->create_collection(), true);
    coll.set<bool>("result", true);
    coll.set<std::string>("contact", _contact_aimid);
    //coll.set<int64_t>("theirs_last_delivered", _state.get_theirs_last_delivered());
    //coll.set<int64_t>("theirs_last_read", _state.get_theirs_last_read());
    //coll.set<int64_t>("last_msg_in_index", _state.get_last_msgid());
    coll.set<std::string>("my_aimid", auth_params_->aimid_);

    serialize_message_ids_4_gui("ids", messages, coll.get(), auth_params_->time_offset_, from_deleted_mode::no, skip_patches::no);

    g_core->post_message_to_gui("messages/received/server", 0, coll.get());

    if (_message_id > 0)
    {
        get_archive()->update_history(_contact_aimid, messages, false, archive::local_history::has_older_message_id::yes)->on_result = []
        (std::shared_ptr<archive::headers_list> _inserted_messages, const archive::dlg_state&, const archive::dlg_state_changes& _changes_on_update, core::archive::storage::result_type _result)
        {
        };
    }
}

void im::delete_archive_message(const int64_t _seq, const std::string &_contact_aimid, const int64_t _message_id, const std::string& _internal_id, const bool _for_all)
{
    assert(_seq > 0);
    assert(!_contact_aimid.empty());

    get_archive()->get_not_sent_message_by_iid(_internal_id)->on_result = [wr_this = weak_from_this(), _seq, _contact_aimid, _message_id, _internal_id, _for_all](archive::not_sent_message_sptr _message)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
        {
            return;
        }

        if (_message && _message->get_message()->is_file_sharing())
        {
            ptr_this->abort_file_sharing_upload(_seq, _contact_aimid, _internal_id);
            coll_helper cl_coll(g_core->create_collection(), true);
            cl_coll.set_value_as_string("contact", _contact_aimid);
            cl_coll.set_value_as_string("id", _internal_id);
            g_core->post_message_to_gui("files_sharing/upload/aborted", 0, cl_coll.get());
            return;
        }

        ptr_this->delete_archive_message_local(_seq, _contact_aimid, _message_id, _internal_id, _for_all);

        __INFO(
            "delete_history",
            "requested to delete a history message\n"
            "    message-id=<%1%>\n"
            "    contact=<%2%>\n"
            "    for_all=<%3%>",
            _message_id % _contact_aimid % logutils::yn(_for_all));

        ptr_this->get_archive()->insert_pending_delete_message(
            _contact_aimid, archive::delete_message(
                _contact_aimid,
                _message_id,
                _internal_id,
                _for_all ? archive::delete_operation::del_for_all : archive::delete_operation::del_for_me));

        ptr_this->post_pending_delete_messages();
    };
}

void im::delete_archive_messages_from(const int64_t _seq, const std::string &_contact_aimid, const int64_t _from_id)
{
    assert(_seq > 0);
    assert(!_contact_aimid.empty());
    assert(_from_id >= 0);

    __INFO(
        "delete_history",
        "requested to delete history messages\n"
        "    from-id=<%1%>\n"
        "    contact=<%2%>",
        _from_id % _contact_aimid
        );

    get_archive()->insert_pending_delete_message(_contact_aimid, archive::delete_message(_contact_aimid, _from_id, std::string(), archive::delete_operation::del_up_to));

    post_pending_delete_messages();

    on_del_up_to_changed(
        _contact_aimid,
        _from_id,
        std::make_shared<auto_callback>([](int32_t){}));
}

void im::delete_archive_all_messages(const int64_t _seq, std::string_view _contact)
{
    get_archive()->get_dlg_state(_contact)->on_result = [wr_this = weak_from_this(), _contact = std::string(_contact), _seq](const archive::dlg_state& _state)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;
        ptr_this->delete_archive_messages_from(_seq, _contact, _state.get_last_msgid());

        ptr_this->clear_last_message_in_dlg_state(_contact);
    };
}

std::string im::_get_protocol_uid() {
    return auth_params_->aimid_;
}

void im::post_voip_msg_to_server(const voip_manager::VoipProtoMsg& msg) {
    auto packet = std::make_shared<core::wim::wim_webrtc>(make_wim_params(), msg);
    auto __this = weak_from_this();

    post_wim_packet(packet)->on_result_ = [packet, __this, msg](int32_t _error)
    {
        std::shared_ptr<wim::im> ptr_this = __this.lock();
        if (!!ptr_this) {
            auto response = packet->getRawData();
            bool success  = _error == 0 && !!response && response->available();
            ptr_this->on_voip_proto_ack(msg, success);
        } else {
            assert(false);
        }
    };
}

std::shared_ptr<wim::wim_packet> im::prepare_voip_msg(const std::string& data)
{
    return std::make_shared<core::wim::wim_allocate>(make_wim_params(), data);
}

std::shared_ptr<wim::wim_packet> im::prepare_voip_pac(const voip_manager::VoipProtoMsg& data)
{
    return std::make_shared<core::wim::wim_webrtc>(make_wim_params(), data);
}

void im::post_voip_alloc_to_server(const std::string& data)
{
    auto packet = std::make_shared<core::wim::wim_allocate>(make_wim_params(), data);
    post_wim_packet(packet)->on_result_ = [packet, wr_this = weak_from_this()](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!!ptr_this) {
            auto response = packet->getRawData();
            if (!!response && response->available()) {
                uint32_t size = response->available();

                auto empty_callback = std::make_shared<auto_callback>([](int32_t){});
                ptr_this->on_voip_proto_msg(true, (const char*)response->read(size), size, std::move(empty_callback));
            }
        } else {
            assert(false);
        }
    };
}


void im::hide_chat_async(const std::string& _contact,  const int64_t _last_msg_id, std::function<void(int32_t)> _on_result)
{
    auto packet = std::make_shared<core::wim::hide_chat>(make_wim_params(), _contact, _last_msg_id);

    post_wim_packet(packet)->on_result_ = [_contact, packet, wr_this = weak_from_this(), _on_result](int32_t _error)
    {
        if (_error == 0)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            ptr_this->active_dialogs_->remove(_contact);
            ptr_this->unfavorite(_contact);
            coll_helper cl_coll(g_core->create_collection(), true);
            cl_coll.set_value_as_string("contact", _contact);
            g_core->post_message_to_gui("active_dialogs_hide", 0, cl_coll.get());
        }

        if (_on_result)
            _on_result(_error);
    };
}


void im::hide_chat(const std::string& _contact)
{
    get_archive()->get_dlg_state(_contact)->on_result = [wr_this = weak_from_this(), _contact](const archive::dlg_state& _state)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->hide_chat_async(_contact, _state.get_last_msgid());
    };
}

void im::mute_chats(std::shared_ptr<std::list<std::string>> _chats)
{
    if (_chats->empty())
    {
        tools::system::delete_file(get_exported_muted_chats_filename());

        return;
    }

    const std::string& chat = _chats->front();

    auto packet = std::make_shared<core::wim::mute_buddy>(make_wim_params(), chat, true);

    post_wim_packet(packet)->on_result_ = [packet, wr_this = weak_from_this(), _chats](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error != wim_protocol_internal_error::wpie_network_error)
        {
            _chats->pop_front();
        }

        ptr_this->mute_chats(_chats);
    };
}

std::shared_ptr<async_task_handlers> im::mute_chat_async(const std::string& _contact, bool _mute)
{
    auto packet = std::make_shared<core::wim::mute_buddy>(make_wim_params(), _contact, _mute);

    return post_wim_packet(packet);
}

void im::mute_chat(const std::string& _contact, bool _mute)
{
    mute_chat_async(_contact, _mute);
}

void im::upload_file_sharing_internal(const archive::not_sent_message_sptr& _not_sent)
{
    assert(_not_sent);

    __TRACE(
        "fs",
        "initiating internal file sharing upload\n"
        "    iid=<%1%>\n"
        "    contact=<%2%>\n"
        "    local_path=<%3%>",
        _not_sent->get_internal_id() %
        _not_sent->get_aimid() %
        _not_sent->get_file_sharing_local_path()
        );

    auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    time -= auth_params_->time_offset_;

    upload_file_params file_params;
    file_params.file_name = core::tools::from_utf8(_not_sent->get_file_sharing_local_path());
    file_params.duration = _not_sent->get_duration();
    file_params.base_content_type = _not_sent->get_base_content_type();
    file_params.locale = g_core->get_locale();

    auto handler = get_loader().upload_file_sharing(
        _not_sent->get_internal_id(),
        std::move(file_params),
        make_wim_params()
        );

    const auto uploading_id = _not_sent->get_internal_id();

    handler->on_result = [wr_this = weak_from_this(), _not_sent, uploading_id](int32_t _error, const web_file_info& _info)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
        {
            return;
        }

        __TRACE(
            "fs",
            "internal file sharing upload completed\n"
            "    iid=<%1%>\n"
            "    contact=<%2%>\n"
            "    filename=<%3%>",
            _not_sent->get_internal_id() %
            _not_sent->get_aimid() %
            _not_sent->get_file_sharing_local_path()
            );

        if (_error == (int32_t)loader_errors::network_error)
        {
            return;
        }

        ptr_this->get_archive()->get_not_sent_message_by_iid(uploading_id)->on_result = [wr_this, _error, uploading_id, _info, _not_sent](archive::not_sent_message_sptr _message)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
            {
                return;
            }

            ptr_this->get_archive()->remove_message_from_not_sent(_not_sent->get_aimid(), true, _not_sent->get_message());

            if (!_message)
                return;

            const auto success = (_error == (int32_t)loader_errors::success);
            if (success)
            {
                auto with_caption = !_message->get_description().empty();
                ptr_this->send_message_to_contact(
                    0,
                    _message->get_aimid(),
                    {
                        _info.get_file_url(),
                        _message->get_quotes(),
                        _message->get_mentions(),
                        _message->get_internal_id(),
                        message_type::file_sharing,
                        0,
                        _message->get_description(),
                        with_caption ? _info.get_file_url() : std::string(),
                        common::tools::patch_version() }
                );

                auto local_path = _message->get_file_sharing_local_path();
                auto url = _info.get_file_url();

                ptr_this->get_async_loader().download_file_sharing_metainfo(url, ptr_this->make_wim_params(), file_sharing_meta_handler_t(
                    [local_path, url, wr_this](loader_errors _error, const file_sharing_meta_data_t& _data)
                {
                    auto ptr_this = wr_this.lock();
                    if (!ptr_this)
                        return;

                    const auto meta_path = ptr_this->get_async_loader().get_meta_path(url, tools::from_utf8(local_path));

                    ptr_this->async_tasks_->run_async_function([local_path, url, meta_path]
                    {
                        core::wim::async_loader::save_filesharing_local_path(meta_path, url, tools::from_utf8(local_path));

                        return 0;

                    });

                }), 0);
            }
        };

        coll_helper cl_coll(g_core->create_collection(), true);
        _info.serialize(cl_coll.get());
        cl_coll.set<int32_t>("error", _error);

        file_sharing_content_type content_type;
        tools::get_content_type_from_uri(_info.get_file_url(), Out content_type);

        cl_coll.set<int32_t>("content_type", static_cast<int32_t>(content_type.type_));
        cl_coll.set<int32_t>("content_subtype", static_cast<int32_t>(content_type.subtype_));

        cl_coll.set<std::string>("local_path", _not_sent->get_file_sharing_local_path());
        cl_coll.set<int64_t>("file_size", _info.get_file_size());
        boost::system::error_code e;
        uint64_t last_modified = boost::filesystem::last_write_time(tools::from_utf8(_not_sent->get_file_sharing_local_path()), e);
        cl_coll.set<int64_t>("last_modified", last_modified);

        cl_coll.set<std::string>("uploading_id", uploading_id);
        g_core->post_message_to_gui("files/upload/result", 0, cl_coll.get());
    };

    handler->on_progress =
        [uploading_id]
    (const web_file_info& _info)
    {
        coll_helper cl_coll(g_core->create_collection(), true);
        _info.serialize(cl_coll.get());
        cl_coll.set<std::string>("uploading_id", uploading_id);
        g_core->post_message_to_gui("files/upload/progress", 0, cl_coll.get());
    };
}


void im::resume_all_file_sharing_uploading()
{
    get_archive()->get_pending_file_sharing()->on_result =
        [wr_this = weak_from_this()](const std::list<archive::not_sent_message_sptr>& _messages)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            for (const auto& _message : _messages)
            {
                ptr_this->resume_file_sharing_uploading(_message);
            }
        };

    get_loader().resume_file_sharing_tasks();
}

void im::resume_stickers_migration()
{
    if (!failed_sticker_migration_.empty())
    {
        get_fs_stickers_by_ids(0, std::move(failed_sticker_migration_));
        failed_sticker_migration_ = {};
    }
}


void im::resume_file_sharing_uploading(const archive::not_sent_message_sptr &_not_sent)
{
    assert(_not_sent);

    if (get_loader().has_file_sharing_task(_not_sent->get_internal_id()))
    {
        return;
    }

    const auto &msg = _not_sent->get_message();

    assert(msg->is_file_sharing());
    assert(msg->get_flags().flags_.outgoing_);

    const auto &fs_data = msg->get_file_sharing_data();

    const std::wstring local_path = fs_data ? tools::from_utf8(fs_data->get_local_path()) : std::wstring();

    const auto file_still_exists = core::tools::system::is_exist(local_path);
    if (!file_still_exists)
    {
        get_archive()->remove_message_from_not_sent(_not_sent->get_aimid(), true, msg);

        return;
    }

    if (has_opened_dialogs(_not_sent->get_aimid()))
        post_not_sent_message_to_gui(0, _not_sent);

    upload_file_sharing_internal(_not_sent);
}

void im::upload_file_sharing(
    int64_t _seq,
    const std::string& _contact,
    const std::string& _file_name,
    std::shared_ptr<core::tools::binary_stream> _data,
    const std::string& _extension,
    const core::archive::quotes_vec& _quotes,
    const std::string& _description,
    const core::archive::mentions_map& _mentions,
    const std::optional<int64_t>& _duration)
{
    std::string file_name = _file_name;
    if (_data->available() > 0)
    {
        const std::wstring path = tools::system::create_temp_file_path() + tools::from_utf8(_extension);
        if (_data->save_2_file(path))
            file_name = tools::from_utf16(path);
    }

    __INFO(
        "fs",
        "initiating file sharing upload\n"
        "    seq=<%1%>\n"
        "    contact=<%2%>\n"
        "    filename=<%3%>",
        _seq % _contact % file_name);

    auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    time -= auth_params_->time_offset_;

    auto quotes = _quotes;
    for (auto& q : quotes)
    {
        int32_t quote_time = q.get_time();
        quote_time -= auth_params_->time_offset_;
        q.set_time(quote_time);
    }

    auto not_sent = archive::not_sent_message::make_outgoing_file_sharing(_contact, time, file_name, quotes, _description, _mentions, _duration);

    if (!not_sent)
    {
        // file is not exist. TODO notification for gui
        return;
    }

    auto handler = get_archive()->insert_not_sent_message(_contact, not_sent);
    handler->on_result_ =
        [wr_this = weak_from_this(), not_sent, _seq, file_name, _contact](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        assert(_error == 0);

        ptr_this->post_not_sent_message_to_gui(_seq, not_sent);

        ptr_this->upload_file_sharing_internal(not_sent);
    };
}

void im::get_file_sharing_preview_size(const int64_t _seq, const std::string& _file_url, const int32_t _original_size)
{
    coll_helper cl_coll(g_core->create_collection(), true);

    cl_coll.set<std::string>("file_url", _file_url);

    std::string file_id;

    if (!tools::parse_new_file_sharing_uri(_file_url, Out file_id))
    {
        cl_coll.set<int32_t>("error", 1);
        write_files_error_in_log("get_file_sharing_preview_size", 1);
        g_core->post_message_to_gui("files/error", _seq, cl_coll.get());
        return;
    }

    assert(!file_id.empty());

    const auto mini_preview_uri = tools::format_file_sharing_preview_uri(file_id, tools::filesharing_preview_size::px192);
    assert(!mini_preview_uri.empty());
    cl_coll.set<std::string>("mini_preview_uri", mini_preview_uri);

    auto file_size = tools::filesharing_preview_size::xlarge;
    if (_original_size != -1)
    {
        const auto& infos = tools::get_available_fs_preview_sizes();
        const auto it = std::find_if(infos.begin(), infos.end(), [_original_size](const auto& _inf) { return _inf.max_side_ >= _original_size; });
        if (it != infos.end())
            file_size = it->size_;
    }

    const auto full_preview_uri = tools::format_file_sharing_preview_uri(file_id, file_size);
    assert(!full_preview_uri.empty());
    cl_coll.set<std::string>("full_preview_uri", full_preview_uri);

    g_core->post_message_to_gui("files/preview_size/result", _seq, cl_coll.get());
}

void im::download_file_sharing_metainfo(const int64_t _seq, const std::string& _file_url)
{
    get_async_loader().download_file_sharing_metainfo(_file_url, make_wim_params(), file_sharing_meta_handler_t(
        [_seq, _file_url, wr_this = weak_from_this()](loader_errors _error, const file_sharing_meta_data_t& _data)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            coll_helper cl_coll(g_core->create_collection(), true);

            if (_error != loader_errors::success)
            {
                cl_coll.set<std::string>("file_url", _file_url);
                cl_coll.set<int32_t>("error", static_cast<int32_t>(_error));
                write_files_error_in_log("download_file_sharing_metainfo", static_cast<int32_t>(_error));
                g_core->post_message_to_gui("files/error", _seq, cl_coll.get());
                return;
            }

            auto meta = _data.additional_data_;

            cl_coll.set_value_as_string("file_name_short", meta->file_name_short_);
            cl_coll.set_value_as_string("file_dlink", meta->file_download_url_);
            cl_coll.set_value_as_int64("file_size", meta->file_size_);
            cl_coll.set_value_as_bool("recognize", meta->recognize_);
            cl_coll.set_value_as_int("duration", meta->duration_);
            cl_coll.set_value_as_bool("got_audio", meta->got_audio_);


            auto local_path = std::make_shared<std::wstring>(tools::from_utf8(meta->local_path_));

            auto on_exit = std::make_shared<core::tools::auto_scope>([wr_this, _data, cl_coll, local_path, _seq]
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                auto saved_by_user = !boost::starts_with(_data.additional_data_->local_path_, ptr_this->get_content_cache_path()) && !_data.additional_data_->local_path_.empty();

                auto coll_copy = cl_coll;

                coll_copy.set_value_as_bool("saved_by_user", saved_by_user);
                coll_copy.set_value_as_string("local_path", tools::from_utf16(*local_path));
                coll_copy.set_value_as_int64("last_modified", _data.additional_data_->last_modified_);

                g_core->post_message_to_gui("files/metainfo/result", _seq, cl_coll.get());
            });

            if (!local_path->empty())
            {
                const auto meta_path = ptr_this->get_async_loader().get_meta_path(_file_url, *local_path);

                ptr_this->async_tasks_->run_async_function([local_path, _data, _file_url, meta_path]
                {
                    auto meta = _data.additional_data_;

                    boost::system::error_code e;
                    uint64_t last_modified = boost::filesystem::last_write_time(*local_path, e);
                    if (!tools::system::is_exist(*local_path) || tools::system::get_file_size(*local_path) != meta->file_size_ || last_modified != meta->last_modified_)
                    {
                        local_path->clear();

                        core::wim::async_loader::save_filesharing_local_path(meta_path, _file_url, *local_path);
                    }

                    return 0;

                })->on_result_ = [on_exit](int32_t _err) {};
            }
        }), _seq);
}

void im::download_file_sharing(const int64_t _seq, const std::string& _contact, const std::string& _file_url, const bool _force_request_metainfo, const std::string& _filename, const std::string& _download_dir, bool _raise_priority)
{
    get_async_loader().set_download_dir(_download_dir.empty() ? get_im_downloads_path(_download_dir) : tools::from_utf8(_download_dir));

    if (_force_request_metainfo)
    {
        tools::system::delete_file(get_path_in_cache(get_content_cache_path(), _file_url, path_type::link_meta));
    }

    auto progress_callback = file_info_handler_t::progress_callback_t([_seq, _file_url](int64_t _total, int64_t _transferred, int32_t _completion_percent)
    {
        coll_helper coll(g_core->create_collection(), true);

        coll.set<std::string>("file_url", _file_url);
        coll.set<int64_t>("file_size", _total);
        coll.set<int64_t>("bytes_transfer", _transferred);

        g_core->post_message_to_gui("files/download/progress", _seq, coll.get());
    });

    auto completion_callback = file_info_handler_t::completion_callback_t(
        [_seq, _file_url, _filename, wr_this = weak_from_this()](loader_errors _error, const file_info_data_t& _data)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        coll_helper coll(g_core->create_collection(), true);

        if (_error != loader_errors::success)
        {
            coll.set<std::string>("file_url", _file_url);
            coll.set<int32_t>("error", static_cast<int32_t>(_error));
            write_files_error_in_log("download_file_sharing", static_cast<int32_t>(_error));
            g_core->post_message_to_gui("files/error", _seq, coll.get());
            return;
        }

        auto on_exit = std::make_shared<core::tools::auto_scope>([wr_this, _data, coll, _file_url, _seq]
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            auto coll_local = coll;

            coll_local.set<std::string>("requested_url", _file_url);
            coll_local.set<std::string>("file_url", _data.additional_data_->url_);
            coll_local.set<std::string>("file_name", tools::from_utf16(_data.additional_data_->local_path_));
            auto saved_by_user = !boost::starts_with(_data.additional_data_->local_path_, ptr_this->get_content_cache_path()) && !_data.additional_data_->local_path_.empty();
            coll_local.set_value_as_bool("saved_by_user", saved_by_user);

            g_core->post_message_to_gui("files/download/result", _seq, coll_local.get());
        });

        const auto meta_path = ptr_this->get_async_loader().get_meta_path(_file_url, _data.additional_data_->local_path_);

        ptr_this->async_tasks_->run_async_function([_data, _file_url, meta_path]
        {
            core::wim::async_loader::save_filesharing_local_path(meta_path, _data.additional_data_->url_, _data.additional_data_->local_path_);

            return 0;
        })->on_result_ = [on_exit](int32_t) {};
    });

    get_async_loader().download_file_sharing(_raise_priority ? highest_priority() : default_priority(), _contact, _file_url, _filename, make_wim_params(), file_info_handler_t(completion_callback, progress_callback));
}

void im::download_image(
    int64_t _seq,
    const std::string& _image_url,
    const std::string& _forced_path,
    const bool _download_preview,
    const int32_t _preview_width,
    const int32_t _preview_height,
    const bool _ext_resource,
    const bool _raise_priority,
    const bool _with_data)
{
    assert(_seq > 0);
    assert(!_image_url.empty());
    assert(_preview_width >= 0);
    assert(_preview_width < 1000);
    assert(_preview_height >= 0);
    assert(_preview_height < 1000);

    __INFO("async_loader",
        "im::download_image\n"
        "seq      = <%1%>\n"
        "url      = <%2%>\n"
        "preview  = <%3%>\n", _seq % _image_url % logutils::yn(_download_preview));

    auto metainfo_handler = link_meta_handler_t([_seq, _image_url](loader_errors _error, const link_meta_data_t& _data)
    {
        __INFO("async_loader",
            "metainfo_handler\n"
            "seq      = <%1%>\n"
            "url      = <%2%>\n"
            "result   = <%3%>\n", _seq % _image_url % static_cast<int>(_error));

        if (_error != loader_errors::success)
            return;

        auto meta = _data.additional_data_;

        const auto width = std::get<0>(meta->get_preview_size());
        const auto height = std::get<1>(meta->get_preview_size());
        const auto origin_width = std::get<0>(meta->get_origin_size());
        const auto origin_height = std::get<1>(meta->get_origin_size());
        const auto& uri = meta->get_download_uri();
        const auto size = meta->get_file_size();
        const auto& content_type = meta->get_content_type();

        coll_helper cl_coll(g_core->create_collection(), true);

        cl_coll.set<int32_t>("preview_width", width);
        cl_coll.set<int32_t>("preview_height", height);
        cl_coll.set<int32_t>("origin_width", origin_width);
        cl_coll.set<int32_t>("origin_height", origin_height);
        cl_coll.set<std::string>("download_uri", uri);
        cl_coll.set<int64_t>("file_size", size);
        cl_coll.set<std::string>("content_type", content_type);

        g_core->post_message_to_gui("image/download/result/meta", _seq, cl_coll.get());
    });

    auto progress_callback = file_info_handler_t::progress_callback_t([_seq](int64_t _total, int64_t _transferred, int32_t _completion_percent)
    {
        coll_helper coll(g_core->create_collection(), true);

        coll.set<int64_t>("bytes_total", _total);
        coll.set<int64_t>("bytes_transferred", _transferred);
        coll.set<int32_t>("pct_transferred", _completion_percent);

        g_core->post_message_to_gui("image/download/progress", _seq, coll.get());
    });

    auto completion_callback = file_info_handler_t::completion_callback_t([_seq, _image_url, _with_data](loader_errors _error, const file_info_data_t& _data)
    {
        __INFO("async_loader",
            "image_handler\n"
            "seq      = <%1%>\n"
            "url      = <%2%>\n"
            "result   = <%3%>\n", _seq % _image_url % static_cast<int>(_error));

        coll_helper cl_coll(g_core->create_collection(), true);

        cl_coll.set<int32_t>("error", static_cast<int32_t>(_error));

        auto file_info = _data.additional_data_;

        const auto& url = file_info->url_;
        cl_coll.set<std::string>("url", url);

        if (_error == loader_errors::success)
        {
            auto image = _data.get_content_as_core_istream(cl_coll);
            cl_coll.set_value_as_stream("data", image.get());

            const auto& local_path = file_info->local_path_;
            cl_coll.set<std::string>("local", tools::from_utf16(local_path));

            cl_coll.set<bool>("with_data", _with_data);
        }

        g_core->post_message_to_gui("image/download/result", _seq, cl_coll.get());
    });

    if (_download_preview)
    {
        get_async_loader().download_image_preview(_raise_priority ? high_priority() : default_priority(),
                                                  _image_url, make_wim_params(), metainfo_handler,
                                                  file_info_handler_t(completion_callback), _seq);
    }
    else
    {
        get_async_loader().download_image(_raise_priority ? high_priority() : default_priority(),
                                          _image_url, _forced_path, make_wim_params(), _ext_resource, _ext_resource,
                                          file_info_handler_t(completion_callback, progress_callback), _seq, _with_data);
    }
}

void im::download_link_preview(
    const int64_t _seq,
    const std::string& _contact_aimid,
    const std::string& _url,
    const int32_t _preview_width,
    const int32_t _preview_height)
{
    assert(_seq > 0);
    assert(!_contact_aimid.empty());
    assert(!_url.empty());
    assert(_preview_width >= 0);
    assert(_preview_width < 1000);
    assert(_preview_height >= 0);
    assert(_preview_height < 1000);

    get_async_loader().download_image_metainfo(_url, make_wim_params(), link_meta_handler_t(
        [_seq, _contact_aimid, _preview_width, _preview_height, wr_this = weak_from_this()](loader_errors _error, const link_meta_data_t _data)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
            {
                return;
            }

            auto meta = _data.additional_data_;

            const bool success = meta && (_error == loader_errors::success);

            coll_helper cl_coll(g_core->create_collection(), true);

            cl_coll.set<bool>("success", success);

            if (success)
            {
                const auto preview_size = meta->get_preview_size();
                const auto o_size = meta->get_origin_size();

                cl_coll.set<std::string>("title", meta->get_title());
                cl_coll.set<std::string>("annotation", meta->get_annotation());
                cl_coll.set<std::string>("site_name", meta->get_site_name());
                cl_coll.set<std::string>("content_type", meta->get_content_type());
                cl_coll.set<std::string>("download_uri", meta->get_download_uri());
                cl_coll.set<int32_t>("preview_width", std::get<0>(preview_size));
                cl_coll.set<int32_t>("preview_height", std::get<1>(preview_size));
                cl_coll.set<int32_t>("origin_width", std::get<0>(o_size));
                cl_coll.set<int32_t>("origin_height", std::get<1>(o_size));

                g_core->execute_core_context([_seq, _contact_aimid, _preview_width, _preview_height, meta, wr_this]()
                {
                    auto ptr_this = wr_this.lock();
                    if (!ptr_this)
                        return;

                    ptr_this->download_link_preview_image(_seq, _contact_aimid, *meta, _preview_width, _preview_height);
                });

                g_core->execute_core_context([_seq, _contact_aimid, meta, wr_this]()
                {
                    auto ptr_this = wr_this.lock();
                    if (!ptr_this)
                        return;

                    ptr_this->download_link_preview_favicon(_seq, _contact_aimid, *meta);
                });
            }

            g_core->post_message_to_gui("link_metainfo/download/result/meta", _seq, cl_coll.get());
        }), _seq);
}

void im::cancel_loader_task(const std::string& _url)
{
    get_async_loader().cancel(_url);
}

void im::abort_file_sharing_download(const std::string& _url)
{
    get_async_loader().cancel_file_sharing(_url);
}

void im::raise_download_priority(const int64_t _task_id)
{
    curl_handler::instance().raise_task(_task_id);
}

void im::contact_switched(const std::string &_contact_aimid)
{
    get_async_loader().contact_switched(_contact_aimid);
}

void im::abort_file_sharing_upload(
    const int64_t _seq,
    const std::string &_contact,
    const std::string &_process_seq)
{
    assert(_seq > 0);
    assert(!_process_seq.empty());
    assert(!_contact.empty());

    auto wr_this = weak_from_this();

    auto history_message = std::make_shared<archive::history_message>();
    history_message->set_internal_id(_process_seq);

    get_archive()->remove_message_from_not_sent(_contact, true, history_message)->on_result_ =
        [wr_this, _process_seq](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->get_loader().abort_file_sharing_process(_process_seq);
    };
}

void im::download_link_preview_image(
    const int64_t _seq,
    const std::string& _contact_aimid,
    const preview_proxy::link_meta &_meta,
    const int32_t _preview_width,
    const int32_t _preview_height)
{
    assert(_seq > 0);
    assert(!_contact_aimid.empty());
    assert(_preview_width >= 0);
    assert(_preview_width < 2000);
    assert(_preview_height >= 0);
    assert(_preview_height < 2000);

    if (!_meta.has_preview_uri())
    {
        return;
    }

    const auto image_uri = _meta.get_preview_uri(_preview_width, _preview_height);

    get_async_loader().download_image(default_priority(), image_uri, make_wim_params(), false, true,
        file_info_handler_t([_seq](loader_errors _error, const file_info_data_t& _data)
        {
            coll_helper cl_coll(g_core->create_collection(), true);

            const auto success = (_error == loader_errors::success);

            cl_coll.set<bool>("success", success);

            if (success)
            {
                auto image = _data.get_content_as_core_istream(cl_coll);
                cl_coll.set_value_as_stream("data", image.get());
            }

            g_core->post_message_to_gui("link_metainfo/download/result/image", _seq, cl_coll.get());
        }), _seq);
}

void im::download_link_preview_favicon(
    const int64_t _seq,
    const std::string& _contact_aimid,
    const preview_proxy::link_meta &_meta)
{
    assert(_seq > 0);
    assert(!_contact_aimid.empty());

    const auto &favicon_uri = _meta.get_favicon_uri();

    if (favicon_uri.empty())
    {
        return;
    }

    const auto favicon_callback = file_info_handler_t([_seq, favicon_uri](loader_errors _error, const file_info_data_t& _data)
    {
        __INFO("async_loader",
            "favicon_callback\n"
            "seq      = <%1%>\n"
            "url      = <%2%>\n"
            "result   = <%3%>\n", _seq % favicon_uri % static_cast<int>(_error));

        coll_helper cl_coll(g_core->create_collection(), true);

        const auto success = (_error == loader_errors::success);
        cl_coll.set<bool>("success", success);

        if (success)
        {
            ifptr<istream> data = _data.get_content_as_core_istream(cl_coll);
            cl_coll.set_value_as_stream("data", data.get());
        }

        g_core->post_message_to_gui("link_metainfo/download/result/favicon", _seq, cl_coll.get());
    });

    get_async_loader().download_image(default_priority(), favicon_uri, make_wim_params(), true, true, favicon_callback);
}

void im::set_played(const std::string& url, bool played)
{
    get_loader().set_played(
        url,
        get_content_cache_path(),
        played,
        make_wim_params());
}

void im::speech_to_text(int64_t _seq, const std::string& _url, const std::string& _locale)
{
    auto wr_this = weak_from_this();

    auto packet = std::make_shared<core::wim::speech_to_text>(make_wim_params(), _url, _locale);

    post_wim_packet(packet)->on_result_ = [wr_this, _seq, packet](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_int("error", _error);
        coll.set_value_as_int("comeback", packet->get_comeback());
        coll.set_value_as_string("text", packet->get_text());
        g_core->post_message_to_gui("files/speech_to_text/result", _seq, coll.get());
    };
}

wim::loader& im::get_loader()
{
    if (!files_loader_)
        files_loader_ = std::make_shared<wim::loader>(get_content_cache_path());

    return *files_loader_;
}

wim::async_loader& im::get_async_loader()
{
    if (!async_loader_)
        async_loader_ = std::make_shared<wim::async_loader>(get_content_cache_path());

    return *async_loader_;
}

void im::search_contacts_server(const int64_t _seq, const std::string_view _keyword, const std::string_view _phone)
{
    auto packet = std::make_shared<search_contacts>(make_wim_params(), _keyword, _phone);

    post_robusto_packet(packet)->on_result_ = [wr_this = weak_from_this(), _seq, packet](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->insert_friendly(packet->get_persons(), friendly_source::remote);

        coll_helper coll(g_core->create_collection(), true);

        if (_error == 0)
            packet->response_.serialize(coll);
        else
            coll.set_value_as_int("error", _error);

        g_core->post_message_to_gui("contacts/search/server/result", _seq, coll.get());
    };
}

void im::syncronize_address_book(const int64_t _seq, const std::string _keyword, const std::vector<std::string> _phone)
{
    auto packet = std::make_shared<sync_ab>(make_wim_params(), _keyword, _phone);

    auto wr_this = weak_from_this();
    post_robusto_packet(packet)->on_result_ = [wr_this, _seq, packet](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        const auto error = packet->get_status_code();
        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_int("error", error);

        g_core->post_message_to_gui("addressbook/sync/update", _seq, coll.get());
    };
}

void im::search_dialogs_one(const int64_t _seq, const std::string_view _keyword, const std::string_view _contact)
{
    auto packet = std::make_shared<search_dialogs>(make_wim_params(), _keyword, _contact);

    auto wr_this = weak_from_this();
    post_robusto_packet(packet)->on_result_ = [wr_this, _seq, packet](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->on_search_dialogs_result(_seq, packet, _error);
    };
}

void im::search_dialogs_all(const int64_t _seq, const std::string_view _keyword)
{
    auto packet = std::make_shared<search_dialogs>(make_wim_params(), _keyword, std::string_view());

    auto wr_this = weak_from_this();
    post_robusto_packet(packet)->on_result_ = [wr_this, _seq, packet](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->on_search_dialogs_result(_seq, packet, _error);
    };
}

void im::search_dialogs_continue(const int64_t _seq, const std::string_view _cursor, const std::string_view _contact)
{
    auto packet = std::make_shared<search_dialogs>(make_wim_params(), std::string_view(), _contact, _cursor);

    auto wr_this = weak_from_this();
    post_robusto_packet(packet)->on_result_ = [wr_this, _seq, packet](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->on_search_dialogs_result(_seq, packet, _error);
    };
}

void im::on_search_dialogs_result(const int64_t _seq, const std::shared_ptr<search_dialogs>& _packet, const int32_t _error)
{
    coll_helper coll(g_core->create_collection(), true);

    insert_friendly(_packet->get_persons(), friendly_source::remote);

    if (_error == 0)
        _packet->results_.serialize(coll, auth_params_->time_offset_);
    else
        coll.set_value_as_int("error", _error);

    g_core->post_message_to_gui("dialogs/search/server/result", _seq, coll.get());
}

void im::get_profile(int64_t _seq, const std::string& _contact)
{
    const auto& aimid = (_contact.empty() ? auth_params_->aimid_ : _contact);
    if (aimid.empty())
        return;

    auto wr_this = weak_from_this();

    auto packet = std::make_shared<core::wim::get_profile>(make_wim_params(), aimid);

    post_wim_packet(packet)->on_result_ = [wr_this, packet, _seq, _contact](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        coll_helper coll(g_core->create_collection(), true);

        if (_error == 0)
        {
            const profile::profiles_list& results = packet->get_result();

            if (results.empty())
            {
                _error = wim_protocol_internal_error::wpie_error_profile_not_found;
            }
            else
            {
                ptr_this->insert_friendly(packet->get_persons(), friendly_source::remote);

                coll_helper coll_profile(coll->create_collection(), true);

                results.front()->serialize(coll_profile);

                coll.set_value_as_collection("profile", coll_profile.get());
            }
        }

        coll.set_value_as_string("aimid", _contact);
        coll.set_value_as_int("error", _error);


        g_core->post_message_to_gui("contacts/profile/get/result", _seq, coll.get());
    };
}

void im::add_contact(int64_t _seq, const std::string& _aimid, const std::string& _group, const std::string& _auth_message)
{
    archive::history_message message;
    message.set_text(_auth_message);
    message.set_internal_id(core::tools::system::generate_internal_id());

    auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    time -= auth_params_->time_offset_;
    message.set_time(time);

    archive::dlg_state state;
    state.set_last_message(message);

    auto wr_this = weak_from_this();

    get_archive()->set_dlg_state(_aimid, state)->on_result = [wr_this, _aimid, _group, _auth_message, _seq](const archive::dlg_state&, const archive::dlg_state_changes&)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->post_dlg_state_to_gui(_aimid);

        auto packet = std::make_shared<core::wim::add_buddy>(ptr_this->make_wim_params(), _aimid, _group, _auth_message);

        ptr_this->post_wim_packet(packet)->on_result_ = [packet, _seq, _aimid](int32_t _error)
        {
            coll_helper coll(g_core->create_collection(), true);
            coll.set_value_as_int("error", _error);
            coll.set_value_as_string("contact", _aimid);

            g_core->post_message_to_gui("contacts/add/result", _seq, coll.get());
        };
    };
}

void im::remove_contact(int64_t _seq, const std::string& _aimid)
{
    auto wr_this = weak_from_this();

    get_archive()->get_dlg_state(_aimid)->on_result = [wr_this, _aimid, _seq](const archive::dlg_state& _state)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        int64_t last_msg_id = _state.get_last_msgid();

        ptr_this->active_dialogs_->remove(_aimid);
        ptr_this->unfavorite(_aimid);

        coll_helper cl_coll(g_core->create_collection(), true);
        cl_coll.set_value_as_string("contact", _aimid);
        g_core->post_message_to_gui("active_dialogs_hide", 0, cl_coll.get());

        ptr_this->get_archive()->clear_dlg_state(_aimid)->on_result_ = [wr_this, _seq, _aimid, last_msg_id](int32_t _err)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            ptr_this->hide_chat_async(_aimid, last_msg_id, [wr_this, _seq, _aimid](int32_t _error)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                auto on_result = [_seq, _aimid](int32_t _error)
                {
                    coll_helper coll(g_core->create_collection(), true);
                    coll.set_value_as_int("error", _error);
                    coll.set_value_as_string("contact", _aimid);

                    g_core->post_message_to_gui("contacts/remove/result", _seq, coll.get());

                    if (_aimid.find("@chat.agent") != _aimid.npos)
                    {
                        g_core->insert_event(core::stats::stats_event_names::chats_leave);
                    }
                };

                auto packet = std::make_shared<core::wim::remove_buddy>(ptr_this->make_wim_params(), _aimid);

                if (ptr_this->contact_list_->exist(_aimid))
                {
                    ptr_this->post_wim_packet(packet)->on_result_ = on_result;
                }
                else
                {
                    on_result(0);
                }
            });
        };
    };
}


void im::rename_contact(int64_t _seq, const std::string& _aimid, const std::string& _friendly)
{
    auto packet = std::make_shared<core::wim::set_buddy_attribute>(make_wim_params(), _aimid, _friendly);

    post_wim_packet(packet)->on_result_ = [_aimid, _friendly, _seq](int32_t _error)
    {
        coll_helper coll(g_core->create_collection(), true);

        coll.set_value_as_int("error", _error);
        coll.set_value_as_string("contact", _aimid);
        coll.set_value_as_string("friendly", _friendly);

        g_core->post_message_to_gui("contacts/rename/result", _seq, coll.get());
    };
}


void im::post_unignored_contact_to_gui(const std::string& _aimid)
{
    if (contact_list_->exist(_aimid))
    {
        coll_helper coll(g_core->create_collection(), true);

        coll.set_value_as_string("type", "created");

        contact_list_->serialize_contact(_aimid, coll.get());

        g_core->post_message_to_gui("contacts/ignore/remove", 0, coll.get());
    }
}


void im::ignore_contact(int64_t _seq, const std::string& _aimid, bool _ignore)
{
    const auto oper = _ignore ? set_permit_deny::operation::ignore : set_permit_deny::operation::ignore_remove;

    auto packet = std::make_shared<core::wim::set_permit_deny>(make_wim_params(), _aimid, oper);

    auto wr_this = weak_from_this();

    post_wim_packet(packet)->on_result_ = [_ignore, wr_this, _aimid, _seq](int32_t _error)
    {
        if (_error == 0)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            if (_ignore)
            {
                ptr_this->contact_list_->add_to_ignorelist(_aimid);

                ptr_this->get_ignore_list(0);
            }
            else
            {
                ptr_this->contact_list_->remove_from_ignorelist(_aimid);

                ptr_this->get_ignore_list(_seq);

                ptr_this->post_unignored_contact_to_gui(_aimid);
            }
        }
    };
}



void im::get_ignore_list(int64_t _seq)
{
    auto packet = std::make_shared<core::wim::get_permit_deny>(make_wim_params());

    post_wim_packet(packet)->on_result_ = [_seq, wr_this = weak_from_this(), packet](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        const auto& info = packet->get_ignore_list();
        ptr_this->insert_friendly(info.get_persons(), friendly_source::remote);

        auto _coll = g_core->create_collection();
        coll_helper coll(_coll, true);

        ifptr<iarray> ignore_array(_coll->create_array());

        for (const auto& sn : info.get_ignore_list())
        {
            coll_helper coll_chatter(coll->create_collection(), true);
            ifptr<ivalue> val(coll->create_value());
            val->set_as_string(sn.c_str(), (int32_t)sn.length());
            ignore_array->push_back(val.get());
        }

        coll.set_value_as_array("aimids", ignore_array.get());
        g_core->post_message_to_gui("contacts/get_ignore/result", _seq, coll.get());
    };
}

void im::favorite(const std::string& _contact)
{
    auto friendly = friendly_container_.get_friendly(_contact).value_or(core::friendly());
    core::wim::favorite fvrt(_contact, std::move(friendly), std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) - auth_params_->time_offset_);
    favorites_->update(fvrt);

    post_dlg_state_to_gui(_contact, true, true, true);

    g_core->insert_event(core::stats::stats_event_names::favorites_set);
}

void im::unfavorite(const std::string& _contact)
{
    favorites_->remove(_contact);

    if (active_dialogs_->contains(_contact))
        post_dlg_state_to_gui(_contact, false, true, true);

    g_core->insert_event(core::stats::stats_event_names::favorites_unset);
}

void im::update_outgoing_msg_count(const std::string& _aimid, int _count)
{
    contact_list_->set_outgoing_msg_count(_aimid, _count);
}

void im::on_event_permit(fetch_event_permit* _event, std::shared_ptr<auto_callback> _on_complete)
{
    contact_list_->update_ignorelist(_event->ignore_list());

    post_ignorelist_to_gui(0);

    _on_complete->callback(0);
}

void im::on_event_imstate(fetch_event_imstate* _event, std::shared_ptr<auto_callback> _on_complete)
{
    auto wr_this = weak_from_this();

    const auto& imstates = _event->get_states();

    for (const auto& _state : imstates)
    {
        if (_state.get_state() == imstate_sent_state::failed)
        {
            get_archive()->failed_pending_message(_state.get_request_id());
        }
        else
        {
            get_archive()->update_pending_messages_by_imstate(
                _state.get_request_id(),
                _state.get_hist_msg_id(),
                _state.get_before_hist_msg_id(),
                _state.get_state() == imstate_sent_state::delivered)->on_result = [wr_this, _state](archive::not_sent_message_sptr _message)
            {

                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                if (_message)
                {
                    coll_helper coll(g_core->create_collection(), true);

                    auto messages_block = std::make_shared<archive::history_block>();
                    messages_block->push_back(_message->get_message());

                    coll.set_value_as_bool("result", true);
                    coll.set_value_as_string("contact", _message->get_aimid());
                    coll.set<std::string>("my_aimid", ptr_this->auth_params_->aimid_);
                    serialize_messages_4_gui("messages", messages_block, coll.get(), ptr_this->auth_params_->time_offset_);

                    g_core->post_message_to_gui("messages/received/message_status", 0, coll.get());

                    ptr_this->get_archive()->update_history(_message->get_aimid(), messages_block)
                        ->on_result = [wr_this, _message](std::shared_ptr<archive::headers_list>, const archive::dlg_state&, const archive::dlg_state_changes&, core::archive::storage::result_type _result)
                        {
                            auto ptr_this = wr_this.lock();
                            if (!ptr_this)
                                return;

                            if (!_result)
                                write_files_error_in_log("update_history", _result.error_code_);

                            ptr_this->post_outgoing_count_to_gui(_message->get_aimid());
                        };
                }
            };
        }
    }

    get_archive()->sync_with_history()->on_result_ = [_on_complete](int32_t _error)
    {
        _on_complete->callback(0);
    };
}

void im::on_event_notification(fetch_event_notification* _event, std::shared_ptr<auto_callback> _on_complete)
{
    auto wr_this = weak_from_this();
    auto notify_callback = [wr_this](core::coll_helper collection, mailbox_change::type type)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        switch (type)
        {
        case core::wim::mailbox_change::status:
            g_core->post_message_to_gui("mailboxes/status", 0, collection.get());
            break;
        case core::wim::mailbox_change::new_mail:
            g_core->post_message_to_gui("mailboxes/new", 0, collection.get());
            break;

        case core::wim::mailbox_change::mail_read:
        default:
            break;
        }
    };

    if (!_event->changes_.empty())
    {
        mailbox_storage_->process(_event->changes_, notify_callback);
    }
    else if (_event->active_dialogs_sent_)
    {
        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_int("dialogs_count", dlg_states_count_);
        g_core->post_message_to_gui("active_dialogs_sent", 0, coll.get());
    }
    _on_complete->callback(0);
}

void im::on_event_appsdata(fetch_event_appsdata* _event, std::shared_ptr<auto_callback> _on_complete)
{
    if (_event->is_refresh_stickers() && !get_strickers_size().empty())
    {
        reload_stickers_meta(0, get_strickers_size());
    }

    _on_complete->callback(0);
}

void im::on_event_mention_me(fetch_event_mention_me* _event, std::shared_ptr<auto_callback> _on_complete)
{
    auto message = _event->get_message();
    auto contact = _event->get_aimid();
    auto time_offset = auth_params_->time_offset_;
    auto my_aimid = auth_params_->aimid_;

    insert_friendly(_event->get_persons(), friendly_source::remote);

    auto wr_this = weak_from_this();

    get_archive()->add_mention(contact, message)->on_result_ = [_on_complete, contact, message, time_offset, my_aimid, wr_this](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        coll_helper coll(g_core->create_collection(), true);

        coll.set_value_as_string("contact", contact);
        coll.set_value_as_string("my_aimid", my_aimid);

        coll_helper msg_coll(g_core->create_collection(), true);

        message->serialize(msg_coll.get(), time_offset, true);

        coll.set_value_as_collection("message", msg_coll.get());

        ptr_this->sync_message_with_dlg_state_queue("mentions/me/received", 0, coll);

        _on_complete->callback(0);
    };
}

void im::on_event_chat_heads(fetch_event_chat_heads* _event, std::shared_ptr<auto_callback> _on_complete)
{
    const auto& heads = _event->get_heads();
    const auto& aimid = heads->chat_aimid_;
    if (heads->reset_state_)
    {
        chat_heads_.erase(aimid);
        serialize_heads(_event->get_heads());
    }
    else
    {
        if (const auto it = chat_heads_.find(aimid); it != chat_heads_.end())
            it->second->merge(*heads);
        else
            chat_heads_[aimid] = heads;

        if (chat_heads_timer_ == empty_timer_id)
        {
            auto wr_this = weak_from_this();
            chat_heads_timer_ = g_core->add_timer([wr_this]
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                for (const auto& [_, h] : ptr_this->chat_heads_)
                    ptr_this->serialize_heads(h);

                g_core->stop_timer(ptr_this->chat_heads_timer_);
                ptr_this->chat_heads_timer_ = empty_timer_id;

            }, std::chrono::seconds(1));
        }
    }

    _on_complete->callback(0);
}

void im::on_event_gallery_notify(fetch_event_gallery_notify* _event, std::shared_ptr<auto_callback> _on_complete)
{
    auto wr_this = weak_from_this();

    auto gallery = _event->get_gallery();
    auto aimid = gallery.get_aimid();
    if (aimid.empty())
    {
        _on_complete->callback(0);
        return;
    }

    if (!has_opened_dialogs(aimid))
    {
        auto state = gallery.get_gallery_state();
        get_archive()->set_gallery_state(aimid, state, false)->on_result_ = [wr_this, aimid, _on_complete, state](int32_t _error)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            ptr_this->post_gallery_state_to_gui(aimid, state);
            _on_complete->callback(0);
        };
        return;
    }


    get_archive()->merge_gallery_from_server(aimid, gallery, archive::gallery_entry_id(), archive::gallery_entry_id())->on_result = [wr_this, aimid, gallery, _on_complete](const std::vector<archive::gallery_item> _changes)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (!_changes.empty())
        {
            coll_helper coll(g_core->create_collection(), true);
            ifptr<iarray> entries_array(coll->create_array());
            entries_array->reserve((int32_t)_changes.size());
            for (const auto& e : _changes)
            {
                coll_helper coll_entry(coll->create_collection(), true);
                e.serialize(coll_entry.get());
                ifptr<ivalue> val(coll->create_value());
                val->set_as_collection(coll_entry.get());
                entries_array->push_back(val.get());
            }

            coll.set_value_as_string("aimid", aimid);
            coll.set_value_as_array("entries", entries_array.get());
            g_core->post_message_to_gui("dialog/gallery/update", 0, coll.get());
        }

        ptr_this->get_archive()->get_gallery_state(aimid)->on_result = [wr_this, aimid, gallery, _on_complete](const archive::gallery_state& _local_state)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            auto finish = [wr_this, aimid, _on_complete](const archive::gallery_state& _state)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                ptr_this->get_archive()->set_gallery_state(aimid, _state, true)->on_result_ = [wr_this, aimid, _on_complete, _state](int32_t _error)
                {
                    auto ptr_this = wr_this.lock();
                    if (!ptr_this)
                        return;

                    ptr_this->post_gallery_state_to_gui(aimid, _state);
                    _on_complete->callback(0);
                    ptr_this->download_gallery_holes(aimid);
                };
            };

            auto new_state = gallery.get_gallery_state();
            if (_local_state.patch_version_ != new_state.patch_version_)
            {
                auto packet = std::make_shared<core::wim::get_dialog_gallery>(ptr_this->make_wim_params(), aimid, _local_state.patch_version_, core::archive::gallery_entry_id(), core::archive::gallery_entry_id(), 0, true);
                ptr_this->post_robusto_packet(packet)->on_result_ = [wr_this, aimid, packet, finish](int32_t _error)
                {
                    auto ptr_this = wr_this.lock();
                    if (!ptr_this)
                        return;

                    auto new_gallery = packet->get_gallery();
                    ptr_this->get_archive()->merge_gallery_from_server(aimid, new_gallery, archive::gallery_entry_id(), archive::gallery_entry_id())->on_result = [wr_this, aimid, finish, new_gallery](const std::vector<archive::gallery_item>& _changes)
                    {
                        auto ptr_this = wr_this.lock();
                        if (!ptr_this)
                            return;

                        if (!_changes.empty())
                        {
                            coll_helper coll(g_core->create_collection(), true);
                            ifptr<iarray> entries_array(coll->create_array());
                            entries_array->reserve((int32_t)_changes.size());
                            for (const auto& e : _changes)
                            {
                                coll_helper coll_entry(coll->create_collection(), true);
                                e.serialize(coll_entry.get());
                                ifptr<ivalue> val(coll->create_value());
                                val->set_as_collection(coll_entry.get());
                                entries_array->push_back(val.get());
                            }

                            coll.set_value_as_string("aimid", aimid);
                            coll.set_value_as_array("entries", entries_array.get());
                            g_core->post_message_to_gui("dialog/gallery/update", 0, coll.get());
                        }

                        if (new_gallery.erased())
                        {
                            coll_helper coll(g_core->create_collection(), true);
                            coll.set_value_as_string("aimid", aimid);
                            g_core->post_message_to_gui("dialog/gallery/erased", 0, coll.get());
                        }

                        finish(new_gallery.get_gallery_state());
                    };
                };
                return;
            }
            else
            {
                finish(new_state);
            }
        };
    };
}

void core::wim::im::on_event_mchat(fetch_event_mchat * _event, std::shared_ptr<auto_callback> _on_complete)
{
    auto sip_code = _event->get_sip_code();
    switch (sip_code)
    {
    case 453:
    {
        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_string("aimid", _event->get_aimid());
        coll.set_value_as_int("sip_code", sip_code);
        g_core->post_message_to_gui("chats/member/add/failed", 0, coll.get());
        break;
    }
    default:
        break;
    }
    _on_complete->callback(0);
}

void im::update_friendly(const friendly_container::friendly_updates& _updates)
{
    if (_updates.empty())
        return;

    coll_helper coll(g_core->create_collection(), true);
    ifptr<iarray> array(coll->create_array());
    array->reserve(_updates.size());
    for (const auto& item : _updates)
    {
        coll_helper coll_item(coll->create_collection(), true);
        coll_item.set_value_as_string("sn", item.uin_);
        coll_item.set_value_as_string("friendly", item.friendly_.name_);
        coll_item.set_value_as_string("nick", item.friendly_.nick_);
        coll_item.set_value_as_bool("official", item.friendly_.official_.value_or(false));
        ifptr<ivalue> val(coll->create_value());
        val->set_as_collection(coll_item.get());
        array->push_back(val.get());
    }
    coll.set_value_as_array("friendly_list", array.get());
    g_core->post_message_to_gui("friendly/update", 0, coll.get());
}

void im::insert_friendly(const std::string& _uin, const std::string& _friendly_name, const std::string& _nick, bool _official, friendly_source _type, friendly_add_mode _mode)
{
    update_friendly(friendly_container_.insert_friendly(_uin, _friendly_name, _nick, _official, _type, _mode));
}

void im::insert_friendly(const std::string& _uin, const std::string& _friendly_name, const std::string& _nick, std::optional<bool> _official, friendly_source _type, friendly_add_mode _mode)
{
    update_friendly(friendly_container_.insert_friendly(_uin, _friendly_name, _nick, _official, _type, _mode));
}

void im::insert_friendly(const core::archive::persons_map& _map, friendly_source _type, friendly_add_mode _mode)
{
    update_friendly(friendly_container_.insert_friendly(_map, _type, _mode));
}

void im::insert_friendly(const std::shared_ptr<core::archive::persons_map>& _map, friendly_source _type, friendly_add_mode _mode)
{
    if (_map)
        update_friendly(friendly_container_.insert_friendly(*_map, _type, _mode));
}

void im::set_resend_gdpr_info(std::unique_ptr<accept_agreement_info> _info)
{
    resend_gdpr_info_ = std::move(_info);
}

void im::send_statistic_if_needed()
{
    if (imstat_->need_send())
    {
        auto packet = imstat_->get_statistic(make_wim_params());
        if (packet)
            post_wim_packet(packet);
    }
}

std::string im::get_login()
{
    return auth_params_->aimid_;
}

void im::check_themes_meta_updates(int64_t _seq)
{
    const auto etag = g_core->load_themes_etag();
    auto packet = std::make_shared<get_themes_index>(make_wim_params(), etag);
    post_wim_packet(packet)->on_result_ = [wr_this = weak_from_this(), packet, etag, _seq](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        const auto send_result_to_gui = [_seq](const int32_t _error)
        {
            assert(_error == 0);

            coll_helper coll(g_core->create_collection(), true);
            coll.set_value_as_int("error", _error);
            g_core->post_message_to_gui("themes/meta/check/result", _seq, coll.get());
        };

        if (_error == 0)
        {
            if (const auto new_etag = packet->get_header_etag(); new_etag != etag)
            {
                g_core->save_themes_etag(new_etag);

                auto response = packet->get_response();
                response->reset_out();
                ptr_this->get_wallpaper_loader()->save(response)->on_result_ = [send_result_to_gui](const bool _res)
                {
                    send_result_to_gui(_res ? 0 : -1);
                };
            }
            else
            {
                send_result_to_gui(0);
            }
        }
        else
        {
            const auto not_modified = packet->get_http_code() == 304;
            send_result_to_gui(not_modified ? 0 : _error);
        }
    };
}

void im::download_wallpapers_recursively()
{
    get_wallpaper_loader()->get_next_download_task()->on_result_ = [wr_this = weak_from_this()](const auto& _task)
    {
        if (!_task)
            return;

        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        const auto file_info_handler = file_info_handler_t([wr_this, url = _task->get_source_url(), id = _task->get_id(), req = _task->is_requested(), is_preview = _task->is_preview()]
        (loader_errors _error, const file_info_data_t& _data)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            ptr_this->get_wallpaper_loader()->on_task_downloaded(url);

            const auto no_loader_errors = _error == loader_errors::success || (_error == loader_errors::http_error && _data.response_code_ == 304);
            if (req && no_loader_errors)
            {
                if (is_preview)
                {
                    ptr_this->get_wallpaper_loader()->get_wallpaper_preview(id)->on_result_ = [wr_this, id](const auto& _wp_data)
                    {
                        if (_wp_data.available() > 0)
                            themes::post_preview_2_gui(id, _wp_data);
                    };
                }
                else
                {
                    ptr_this->get_wallpaper_loader()->get_wallpaper_full(id)->on_result_ = [wr_this, id](const auto& _wp_data)
                    {
                        if (_wp_data.available() > 0)
                            themes::post_wallpaper_2_gui(id, _wp_data);
                    };
                }
            }

            ptr_this->download_wallpapers_recursively();
        });

        ptr_this->get_async_loader().download_file(
            default_priority(),
            _task->get_source_url(),
            _task->get_source_url(),
            _task->get_dest_file(),
            ptr_this->make_wim_params(),
            file_info_handler,
            _task->get_last_modified_time(),
            -1,
            true,
            _task->get_endpoint());
    };
}

void im::get_theme_wallpaper(const std::string_view _wp_id)
{
    get_wallpaper_loader()->get_wallpaper_full(_wp_id)->on_result_ = [wr_this = weak_from_this(), id = std::string(_wp_id)](const auto& _wp_data)
    {
        if (_wp_data.available() > 0)
        {
            themes::post_wallpaper_2_gui(id, _wp_data);
        }
        else
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            ptr_this->download_wallpapers_recursively();
        }
    };
}

void im::get_theme_wallpaper_preview(const std::string_view _wp_id)
{
    get_wallpaper_loader()->get_wallpaper_preview(_wp_id)->on_result_ = [wr_this = weak_from_this(), id = std::string(_wp_id)](const auto& _wp_data)
    {
        if (_wp_data.available() > 0)
        {
            themes::post_preview_2_gui(id, _wp_data);
        }
        else
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            ptr_this->download_wallpapers_recursively();
        }
    };
}

void im::set_user_wallpaper(const std::string_view _wp_id, tools::binary_stream _image)
{
    get_wallpaper_loader()->save_wallpaper(_wp_id, std::make_shared<core::tools::binary_stream>(std::move(_image)));
}

void im::remove_user_wallpaper(const std::string_view _wp_id)
{
    get_wallpaper_loader()->remove_wallpaper(_wp_id);
}

void im::update_profile(int64_t _seq, const std::vector<std::pair<std::string, std::string>>& _field)
{
    auto packet = std::make_shared<core::wim::update_profile>(make_wim_params(), _field);

    post_wim_packet(packet)->on_result_ = [packet, _seq, wr_this = weak_from_this()](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_int("error", _error);
        g_core->post_message_to_gui("update_profile/result", _seq, coll.get());
    };
}

void im::join_live_chat(int64_t _seq, const std::string& _stamp)
{
    auto packet = std::make_shared<core::wim::join_chat_alpha>(make_wim_params(), _stamp);

    post_robusto_packet(packet)->on_result_ = [wr_this = weak_from_this(), _seq, packet](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_int("error", _error);
        g_core->post_message_to_gui("livechat/join/result", _seq, coll.get());
    };
}

std::shared_ptr<async_task_handlers> im::send_timezone()
{
    auto handler = std::make_shared<async_task_handlers>();
    auto packet = std::make_shared<core::wim::set_timezone>(make_wim_params());

    post_wim_packet(packet)->on_result_ = [handler](int32_t _error)
    {
        if (handler->on_result_)
            handler->on_result_(_error);
    };

    return handler;
}

void im::set_avatar(const int64_t _seq, tools::binary_stream image, const std::string& _aimId, const bool _chat)
{
    auto packet = std::make_shared<core::wim::set_avatar>(make_wim_params(), image, _aimId, _chat);

    post_wim_packet(packet)->on_result_ = [packet, _seq, wr_this = weak_from_this(), _aimId](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error != 0)
        {
            if (_aimId.empty())
            {
                g_core->insert_event(core::stats::stats_event_names::introduce_avatar_fail);
            }
        }
        else
        {
            if (_aimId.empty()) // self avatar
            {
                const auto aimid = ptr_this->make_wim_params().aimid_;
                ptr_this->get_avatar_loader()->remove_contact_avatars(aimid, ptr_this->get_avatars_data_path())->on_result_ = [aimid](int32_t _error)
                {
                    coll_helper coll(g_core->create_collection(), true);
                    coll.set_value_as_string("aimid", aimid);

                    g_core->post_message_to_gui("avatars/presence/updated", 0, coll.get());
                };
            }
        }

        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_int("error", _error);
        coll.set_value_as_int64("seq", _seq);
        coll.set_value_as_string("id", packet->get_id());
        g_core->post_message_to_gui("set_avatar/result", _seq, coll.get());
    };
}

void im::post_presence_to_gui(const std::string& _aimid)
{
    auto presence = contact_list_->get_presence(_aimid);
    if (presence)
    {
        coll_helper cl_coll(g_core->create_collection(), true);
        cl_coll.set_value_as_string("aimId", _aimid);
        presence->serialize(cl_coll.get());
        g_core->post_message_to_gui("contact/presence", 0, cl_coll.get());
    }
}

void core::wim::im::post_outgoing_count_to_gui(const std::string & _aimid)
{
    auto presence = contact_list_->get_presence(_aimid);
    if (presence)
    {
        coll_helper cl_coll(g_core->create_collection(), true);
        cl_coll.set_value_as_string("aimid", _aimid);
        cl_coll.set_value_as_int("outgoing_count", presence->outgoing_msg_count_);
        g_core->post_message_to_gui("contact/outgoing_count", 0, cl_coll.get());
    }
}

bool im::has_valid_login() const
{
    return auth_params_->is_valid();
}

void im::merge_exported_account(core::wim::auth_parameters& _params)
{
    auto merge_params = std::make_shared<core::wim::auth_parameters>(_params);

    if (merge_params->password_md5_.length())
    {
        auto packet = std::make_shared<client_login>(make_wim_params(),
                                                     merge_params->aimid_,
                                                     merge_params->password_md5_);

        post_wim_packet(packet)->on_result_ = [wr_this = weak_from_this(), merge_params, packet](int32_t _error)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            if (_error == 0)
            {
                merge_params->a_token_ = packet->get_a_token();
                merge_params->session_key_ = packet->get_session_key();
                merge_params->exipired_in_ = packet->get_expired_in();
                merge_params->time_offset_ = packet->get_time_offset();
                merge_params->aimsid_ = std::string();
                merge_params->password_md5_ = std::string();

                ptr_this->merge_account(*merge_params);
            }
        };
    }
    else
    {
        merge_account(*merge_params);
    }
}

void im::get_code_by_phone_call(const std::string& _ivr_url)
{
    auto packet = std::make_shared<core::wim::get_code_by_phone_call>(make_wim_params(), _ivr_url);
    post_wim_packet(packet);
}

void im::get_attach_phone_info(const int64_t _seq, const std::string& _locale, const std::string& _lang)
{
    std::string product = core::utils::get_client_string();
    std::string platform = core::utils::get_protocol_platform_string();

    auto packet = std::make_shared<core::wim::get_attach_phone_info>(make_wim_params(), product, platform, _locale, _lang, tools::version_info().get_build_version());

    post_wim_packet(packet)->on_result_ = [wr_this = weak_from_this(), packet, _seq](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == 0)
        {
            coll_helper cl_coll(g_core->create_collection(), true);
            cl_coll.set_value_as_string("aimid", ptr_this->auth_params_->aimid_);
            cl_coll.set_value_as_bool("show", packet->get_show());
            cl_coll.set_value_as_bool("close", packet->get_close());
            cl_coll.set_value_as_int("timeout", packet->get_timeout());
            cl_coll.set_value_as_string("text", packet->get_text());
            cl_coll.set_value_as_string("title", packet->get_title());
            g_core->post_message_to_gui("attach_phone_info", _seq, cl_coll.get());
        }
    };
}

void im::get_voip_calls_quality_popup_conf(const int64_t _seq, const std::string &_locale, const std::string &_lang)
{
    std::string product = core::utils::get_client_string();
    std::string platform = core::utils::get_protocol_platform_string();

    auto packet = std::make_shared<core::wim::get_voip_calls_quality_popup_conf>(make_wim_params(), product, platform, _locale, _lang, tools::version_info().get_build_version());

    post_wim_packet(packet)->on_result_ = [wr_this = weak_from_this(), packet, _seq](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        coll_helper cl_coll(g_core->create_collection(), true);

        if (_error == 0)
            packet->get_popup_configuration().serialize(cl_coll.get());
        else
            cl_coll.set_value_as_int("error", 1);

        g_core->post_message_to_gui("voip_calls_quality_popup_conf",
                                    _seq,
                                    cl_coll.get());
    };
}

void im::get_logs_path(const int64_t _seq)
{
    coll_helper cl_coll(g_core->create_collection(), true);
    cl_coll.set_value_as_string("path", utils::get_logs_path().string());

    g_core->post_message_to_gui("logs_path", _seq, cl_coll.get());
}

void im::remove_content_cache(const int64_t _seq)
{
    coll_helper cl_coll(g_core->create_collection(), true);

    auto content_cache_path = get_content_cache_path();
    boost::filesystem::path path_to_remove(content_cache_path);

    int32_t error = core::utils::remove_dir_contents_recursively(path_to_remove).value();
    cl_coll.set_value_as_int("last_error", error);

    g_core->post_message_to_gui("content_cache_removed", _seq, cl_coll.get());
}

void im::clear_avatars(const int64_t _seq)
{
    coll_helper cl_coll(g_core->create_collection(), true);

    auto avatars_data_path = get_avatars_data_path();
    boost::filesystem::path path_to_remove(avatars_data_path);

    int32_t error = core::utils::remove_dir_contents_recursively(path_to_remove).value();
    cl_coll.set_value_as_int("last_error", error);

    g_core->post_message_to_gui("avatars_folder_cleared", _seq, cl_coll.get());
}

void im::report_contact(int64_t _seq, const std::string& _aimid, const std::string& _reason, const bool _ignore_and_close)
{
    auto packet = std::make_shared<core::wim::report_contact>(make_wim_params(), _aimid, core::wim::get_reason_by_string(_reason));

    std::function<void(int32_t)> on_result;
    if (_ignore_and_close)
    {
        auto wr_this = weak_from_this();
        on_result = [wr_this, _seq, _aimid](int32_t _error)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            auto packet = std::make_shared<core::wim::set_permit_deny>(ptr_this->make_wim_params(), _aimid, set_permit_deny::operation::ignore);
            ptr_this->post_wim_packet(packet)->on_result_ = [wr_this, _aimid, _seq](int32_t _error)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                ptr_this->remove_contact(_seq, _aimid);
            };
        };
    }

    post_robusto_packet(packet)->on_result_ = on_result;
}

void im::report_stickerpack(const int32_t _id, const std::string& _reason)
{
    auto packet = std::make_shared<core::wim::report_stickerpack>(make_wim_params(), _id, core::wim::get_reason_by_string(_reason));
    post_robusto_packet(packet);
}

void im::report_sticker(const std::string& _id, const std::string& _reason, const std::string& _aimid, const std::string& _chatId)
{
    auto packet = std::make_shared<core::wim::report_sticker>(make_wim_params(), _id, core::wim::get_reason_by_string(_reason), _aimid, _chatId);
    post_robusto_packet(packet);
}

void im::report_message(const int64_t _id, const std::string& _text, const std::string& _reason, const std::string& _aimid, const std::string& _chatId)
{
    auto packet = std::make_shared<core::wim::report_message>(make_wim_params(), _id, _text, core::wim::get_reason_by_string(_reason), _aimid, _chatId);
    post_robusto_packet(packet);
}

void im::user_accept_gdpr(int64_t _seq, const accept_agreement_info &_accept_info)
{
    core::configuration::set_config_option(core::configuration::app_config::AppConfigOption::gdpr_user_has_agreed,
                                           _accept_info.action_ == accept_agreement_info::agreement_action::accept);
    core::configuration::set_config_option(core::configuration::app_config::AppConfigOption::gdpr_agreement_reported_to_server,
        static_cast<int32_t>(core::configuration::app_config::gdpr_report_to_server_state::sent));

    const auto app_ini_path = utils::get_app_ini_path();
    configuration::dump_app_config_to_disk(app_ini_path);


    core::coll_helper cl_coll(g_core->create_collection(), true);
    configuration::get_app_config().serialize(Out cl_coll);
    g_core->post_message_to_gui("app_config", 0, cl_coll.get());

    send_user_agreement(_accept_info, _seq);
}

void im::send_stat()
{
    auto packet = std::make_shared<core::wim::send_stat>(make_wim_params(), get_audio_devices_names(), get_video_devices_names());
    post_wim_packet(packet);

    const auto current = std::chrono::system_clock::now();
    g_core->set_last_stat_send_time(std::chrono::system_clock::to_time_t(current));
}

void im::schedule_stat_timer(bool _first_send)
{
    using dest_duration = std::chrono::seconds;

    auto timeout = std::chrono::duration_cast<dest_duration>(send_stat_interval);

    if (_first_send)
    {
        auto last_send_time = std::chrono::system_clock::from_time_t(g_core->get_last_stat_send_time());
        const auto current = std::chrono::system_clock::now();

        timeout -= std::chrono::duration_cast<dest_duration>(current - last_send_time);
    }

    stat_timer_id_ = g_core->add_timer([wr_this = weak_from_this(), _first_send]
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->send_stat();

        if (_first_send)
        {
            ptr_this->stop_stat_timer();
            ptr_this->schedule_stat_timer(false);
        }

    }, timeout);
}

void im::stop_stat_timer()
{
    if (g_core && stat_timer_id_ != empty_timer_id)
        g_core->stop_timer(stat_timer_id_);
    stat_timer_id_ = empty_timer_id;
}

void im::request_memory_usage(const int64_t _seq)
{
    static memory_stats::requested_types required_stat_types = {
           memory_stats::stat_type::cached_avatars,
           memory_stats::stat_type::cached_themes,
           memory_stats::stat_type::cached_emojis,
           memory_stats::stat_type::cached_previews,
           memory_stats::stat_type::cached_stickers,
           memory_stats::stat_type::voip_initialization,
           memory_stats::stat_type::video_player_initialization,
    };

    auto request_handle = memory_stats_collector_->request_memory_usage(required_stat_types);
    assert(request_handle.is_valid());

    coll_helper cl_coll(g_core->create_collection(), true);
    cl_coll.set_value_as_int64("request_id", request_handle.id_);

    g_core->post_message_to_gui("request_memory_usage_accepted", _seq, cl_coll.get());
}

void im::report_memory_usage(const int64_t _seq,
                             core::memory_stats::request_id _req_id,
                             const core::memory_stats::partial_response& _partial_response)
{
    bool response_ready = memory_stats_collector_->update_response(_req_id, _partial_response);
    if (!response_ready)
        return;

    auto response = memory_stats_collector_->get_response(_req_id);
    coll_helper cl_coll(g_core->create_collection(), true);
    response.serialize(cl_coll);
    cl_coll.set_value_as_int64("request_id", _req_id);

    memory_stats_collector_->finish_request(_req_id);

    g_core->post_message_to_gui("memory_usage_report_ready", _seq, cl_coll.get());
}

void im::get_ram_usage(const int64_t _seq)
{
    get_archive()->get_memory_usage()->on_result = [_seq](const int64_t _index_memory_usage, const int64_t _gallery_memory_usage)
    {
        coll_helper cl_coll(g_core->create_collection(), true);

        cl_coll.set_value_as_int64("ram_used", core::utils::get_current_process_ram_usage());
        cl_coll.set_value_as_int64("real_mem", core::utils::get_current_process_real_mem_usage());
        cl_coll.set_value_as_int64("archive_index", _index_memory_usage);
        cl_coll.set_value_as_int64("archive_gallery", _gallery_memory_usage);
        cl_coll.set_value_as_int64("voip_initialization", g_core->get_voip_init_memory_usage());

        g_core->post_message_to_gui("ram_usage_response", _seq, cl_coll.get());
    };
}

void im::make_archive_holes(const int64_t _seq, const std::string& _archive)
{
    get_archive()->make_holes(_archive);
}

void im::invalidate_archive_data(const int64_t _seq, const std::string& _archive, std::vector<int64_t> _ids)
{
    get_archive()->invalidate_message_data(_archive, std::move(_ids));
}

void im::invalidate_archive_data(const int64_t _seq, const std::string& _archive, int64_t _from, int64_t _before_count, int64_t _after_count)
{
    get_archive()->invalidate_message_data(_archive, _from, _before_count, _after_count);
}

void im::serialize_heads(const chat_heads_sptr& _heads)
{
    if (!_heads)
        return;

    coll_helper coll(g_core->create_collection(), true);

    coll.set_value_as_string("sn", _heads->chat_aimid_);
    coll.set_value_as_bool("reset_state", _heads->reset_state_);

    insert_friendly(_heads->persons_, friendly_source::remote);

    ifptr<iarray> heads_array(coll->create_array());
    heads_array->reserve(_heads->chat_heads_.size());

    for (const auto&[id, uins] : _heads->chat_heads_)
    {
        if (uins.empty())
            continue;

        coll_helper coll_head(coll->create_collection(), true);
        coll_head.set_value_as_int64("msgId", id);
        ifptr<iarray> sn_array(coll->create_array());
        sn_array->reserve(uins.size());
        for (const auto& s : uins)
        {
            ifptr<ivalue> val(coll->create_value());
            val->set_as_string(s.c_str(), (int32_t)s.size());
            sn_array->push_back(val.get());
        }
        coll_head.set_value_as_array("people", sn_array.get());

        ifptr<ivalue> val(coll->create_value());
        val->set_as_collection(coll_head.get());
        heads_array->push_back(val.get());
    }

    coll.set_value_as_array("heads", heads_array.get());
    g_core->post_message_to_gui("chat/heads", 0, coll.get());
}

void im::get_dialog_gallery(const int64_t _seq,
                            const std::string& _aimid,
                            const std::vector<std::string>& _type,
                            const archive::gallery_entry_id _after,
                            const int _page_size)
{
    get_archive()->get_gallery_entries(_aimid, _after, _type, _page_size)->on_result = [wr_this = weak_from_this(), _seq, _aimid, _type](const std::vector<archive::gallery_item>& _entries, bool _exhausted)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        coll_helper coll(g_core->create_collection(), true);

        ifptr<iarray> entries_array(coll->create_array());
        if (!_entries.empty())
        {
            entries_array->reserve((int32_t)_entries.size());
            for (const auto& e : _entries)
            {
                coll_helper coll_entry(coll->create_collection(), true);
                e.serialize(coll_entry.get());
                ifptr<ivalue> val(coll->create_value());
                val->set_as_collection(coll_entry.get());
                entries_array->push_back(val.get());
            }
        }

        coll.set_value_as_array("entries", entries_array.get());
        coll.set_value_as_bool("exhausted", _exhausted);
        g_core->post_message_to_gui("dialog/gallery/get/result", _seq, coll.get());

        ptr_this->download_gallery_holes(_aimid);
    };
}

void im::get_dialog_gallery_by_msg(const int64_t _seq, const std::string& _aimid, const std::vector<std::string>& _type, int64_t _msg_id)
{
    get_archive()->get_gallery_entries_by_msg(_aimid, _type, _msg_id)->on_result = [wr_this = weak_from_this(), _seq, _aimid, _type](const std::vector<archive::gallery_item>& _entries, int _index, int _total)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        coll_helper coll(g_core->create_collection(), true);

        ifptr<iarray> entries_array(coll->create_array());
        if (!_entries.empty())
        {
            entries_array->reserve((int32_t)_entries.size());
            for (const auto& e : _entries)
            {
                coll_helper coll_entry(coll->create_collection(), true);
                e.serialize(coll_entry.get());
                ifptr<ivalue> val(coll->create_value());
                val->set_as_collection(coll_entry.get());
                entries_array->push_back(val.get());
            }
        }

        coll.set_value_as_array("entries", entries_array.get());
        coll.set_value_as_int("index", _index);
        coll.set_value_as_int("total", _total);
        g_core->post_message_to_gui("dialog/gallery/get_by_msg/result", _seq, coll.get());

        ptr_this->download_gallery_holes(_aimid);
    };
}

void im::request_gallery_state(const std::string& _aimId)
{
    get_archive()->get_gallery_state(_aimId)->on_result = [wr_this = weak_from_this(), _aimId](const archive::gallery_state& _state)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->post_gallery_state_to_gui(_aimId, _state);
        ptr_this->download_gallery_holes(_aimId);
    };
}

void im::get_gallery_index(const std::string& _aimId, const std::vector<std::string>& _type, int64_t _msg, int64_t _seq)
{
    get_archive()->get_gallery_entries_by_msg(_aimId, _type, _msg)->on_result = [wr_this = weak_from_this(), _aimId, _msg, _seq](const std::vector<archive::gallery_item>& _entries, int _index, int _total)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        for (const auto& e : _entries)
        {
            ++_index;
            if (e.id_.seq_ == _seq)
                break;
        }

        coll_helper coll(g_core->create_collection(), true);
        ifptr<iarray> entries_array(coll->create_array());
        coll.set_value_as_int("index", _index);
        coll.set_value_as_int("total", _total);
        coll.set_value_as_string("aimid", _aimId);
        coll.set_value_as_int64("msg", _msg);
        coll.set_value_as_int64("seq", _seq);
        g_core->post_message_to_gui("dialog/gallery/index", 0, coll.get());
    };
}

void im::make_gallery_hole(const std::string& _aimid, int64_t _from, int64_t _till)
{
    get_archive()->make_gallery_hole(_aimid, _from, _till)->on_result_ = [](int32_t) {};
}

void im::stop_ui_activity_timer()
{
    if ((ui_activity_timer_id_ > 0) && g_core)
    {
        g_core->stop_timer(ui_activity_timer_id_);
    }
}

void im::check_ui_activity()
{
    const auto current_time = std::chrono::system_clock::now();

    if (current_time - last_ui_activity_ > std::chrono::seconds(5))
    {
        if (ui_active_)
        {
            ui_active_ = false;

            set_state(0, core::profile_state::offline);
        }
    }
    else if (!ui_active_)
    {
        ui_active_ = true;

        set_state(0, core::profile_state::online);
    }
}

void im::schedule_ui_activity_timer()
{
    constexpr auto timeout = std::chrono::seconds(5);

    ui_activity_timer_id_ = g_core->add_timer([wr_this = weak_from_this()]
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        ptr_this->check_ui_activity();

    }, timeout);
}


void im::on_ui_activity(const int64_t _time)
{
    const auto ui_activity_time = std::chrono::system_clock::from_time_t(_time);

    last_ui_activity_ = ui_activity_time;
}

void im::close_stranger(const std::string& _contact)
{
    get_archive()->get_dlg_state(_contact)->on_result = [wr_this = weak_from_this(), _contact](const archive::dlg_state& _local_dlg_state)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_local_dlg_state.get_fake())
            return;

        archive::dlg_state new_dlg_state = _local_dlg_state;
        new_dlg_state.set_stranger(false);

        ptr_this->get_archive()->set_dlg_state(_contact, new_dlg_state)->on_result =
            [wr_this, _contact]
        (const archive::dlg_state &_local_dlg_state, const archive::dlg_state_changes&)
        {
            auto ptr_this = wr_this.lock();
            if (!ptr_this)
                return;

            set_dlg_state_params params;
            params.aimid_ = _contact;
            params.stranger_ = _local_dlg_state.get_stranger();

            ptr_this->set_dlg_state(std::move(params))->on_result_ = [wr_this, _contact](int32_t error)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                ptr_this->post_dlg_state_to_gui(_contact);
            };
        };
    };
}

void im::on_local_pin_set(const std::string& _password)
{
    local_pin_hash_ = utils::calc_local_pin_hash(_password, g_core->get_uniq_device_id());

    g_core->set_local_pin(_password);
    g_core->set_local_pin_enabled(true);

    store_auth_parameters();
}

void im::on_local_pin_entered(const int64_t _seq, const std::string& _password)
{
    auto correct = g_core->verify_local_pin(_password);

    coll_helper coll(g_core->create_collection(), true);
    coll.set_value_as_bool("result", correct);

    g_core->post_message_to_gui("localpin/checked", _seq, coll.get());

    if (correct && std::exchange(waiting_for_local_pin_, false))
    {
        local_pin_hash_ = utils::calc_local_pin_hash(_password, g_core->get_uniq_device_id());
        load_auth_and_fetch_parameters();
    }
}

void im::on_local_pin_disable(const int64_t _seq)
{
    g_core->set_local_pin_enabled(false);
    store_auth_parameters();
}

void im::get_id_info(const int64_t _seq, const std::string_view _id)
{
    auto packet = std::make_shared<core::wim::get_id_info>(make_wim_params(), _id);

    post_robusto_packet(packet)->on_result_ = [wr_this = weak_from_this(), packet, _seq](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;


        coll_helper coll(g_core->create_collection(), true);
        if (_error == 0)
        {
            ptr_this->insert_friendly(packet->get_persons(), core::friendly_source::remote);
            packet->get_response().serialize(coll);
        }
        else
        {
            coll.set_value_as_int("error", _error);
        }

        g_core->post_message_to_gui("idinfo/get/result", _seq, coll.get());
    };
}

void im::get_user_info(const int64_t _seq, const std::string& _aimid)
{
    auto packet = std::make_shared<core::wim::get_user_info>(make_wim_params(), _aimid);

    post_wim_packet(packet)->on_result_ = [wr_this = weak_from_this(), packet, _seq, _aimid](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        coll_helper coll(g_core->create_collection(), true);

        if (_error == 0)
        {
            ptr_this->insert_friendly(packet->get_persons(), core::friendly_source::remote);
            packet->get_info().serialize(coll);
        }

        coll.set_value_as_string("aimid", _aimid);
        coll.set_value_as_int("error", _error);

        g_core->post_message_to_gui("get_user_info/result", _seq, coll.get());
    };
}

void im::set_privacy_settings(const int64_t _seq, privacy_settings _settings)
{
    auto packet = std::make_shared<core::wim::set_privacy_settings>(make_wim_params(), std::move(_settings));

    post_wim_packet(packet)->on_result_ = [wr_this = weak_from_this(), packet, _seq](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        coll_helper coll(g_core->create_collection(), true);
        coll.set_value_as_int("error", _error);

        g_core->post_message_to_gui("privacy_settings/set/result", _seq, coll.get());
    };
}

void im::get_privacy_settings(const int64_t _seq)
{
    auto packet = std::make_shared<core::wim::get_privacy_settings>(make_wim_params());

    post_wim_packet(packet)->on_result_ = [wr_this = weak_from_this(), packet, _seq](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        coll_helper coll(g_core->create_collection(), true);
        if (_error == 0)
            packet->get_settings().serialize(coll);
        else
            coll.set_value_as_int("error", _error);

        g_core->post_message_to_gui("privacy_settings/get/result", _seq, coll.get());
    };
}

void im::get_user_last_seen(const int64_t _seq, const std::vector<std::string>& _ids)
{
    auto packet = std::make_shared<core::wim::get_user_last_seen>(make_wim_params(), _ids);

    post_wim_packet(packet)->on_result_ = [wr_this = weak_from_this(), packet, _seq](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        coll_helper coll(g_core->create_collection(), true);
        if (_error == 0)
        {
            const auto& result = packet->get_result();
            ifptr<iarray> arr(coll->create_array());
            arr->reserve(result.size());

            for (const auto& p : result)
            {
                coll_helper coll_seen(coll->create_collection(), true);

                coll_seen.set_value_as_string("aimid", p.first);
                coll_seen.set_value_as_int("lastseen", p.second);

                ifptr<ivalue> val(coll->create_value());
                val->set_as_collection(coll_seen.get());
                arr->push_back(val.get());
            }

            coll.set_value_as_array("ids", arr.get());
        }

        coll.set_value_as_int("error", _error);

        g_core->post_message_to_gui("get_user_last_seen/result", _seq, coll.get());
    };
}

void im::on_nickname_check(const int64_t _seq, const std::string& _nick, bool _set_nick)
{
    auto packet = std::make_shared<core::wim::check_nick>(make_wim_params(), _nick, _set_nick);

    auto wr_this = weak_from_this();
    post_wim_packet(packet)->on_result_ = [wr_this, _seq, packet](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        coll_helper coll(g_core->create_collection(), true);

        if (_error != 0)
        {
            switch (_error)
            {
            case wpie_error_nickname_bad_value:
                _error = static_cast<int32_t>(core::nickname_errors::bad_value);
                break;
            case wpie_error_nickname_already_used:
                _error = static_cast<int32_t>(core::nickname_errors::already_used);
                break;
            case wpie_error_nickname_not_allowed:
                _error = static_cast<int32_t>(core::nickname_errors::not_allowed);
                break;
            default:
                _error = static_cast<int32_t>(core::nickname_errors::unknown_error);
                break;
            }
        }

        coll.set_value_as_int("error", _error);
        g_core->post_message_to_gui("nickname/check/set/result", _seq, coll.get());
    };
}

void im::on_get_common_chats(const int64_t _seq, const std::string& _sn)
{
    auto packet = std::make_shared<core::wim::get_common_chats>(make_wim_params(), _sn);

    auto wr_this = weak_from_this();
    post_wim_packet(packet)->on_result_ = [wr_this, _seq, packet](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        coll_helper coll(g_core->create_collection(), true);
        if (_error == 0)
        {
            ifptr<iarray> chats_array(coll->create_array());
            if (!packet->result_.empty())
            {
                chats_array->reserve((int32_t)packet->result_.size());
                for (const auto& chat : packet->result_)
                {
                    coll_helper coll_message(coll->create_collection(), true);
                    chat.serialize(coll_message);
                    ifptr<ivalue> val(coll->create_value());
                    val->set_as_collection(coll_message.get());
                    chats_array->push_back(val.get());
                }
            }
            coll.set_value_as_array("chats", chats_array.get());
        }
        coll.set_value_as_int("error", _error);
        g_core->post_message_to_gui("common_chats/get/result", _seq, coll.get());
    };
}
