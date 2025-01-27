#include "stdafx.h"


#include <algorithm>


#include "avatar_loader.h"
#include "../../async_task.h"
#include "../../tools/system.h"

#include "packets/request_avatar.h"

#include <boost/range/adaptor/reversed.hpp>

using namespace core;
using namespace wim;

//////////////////////////////////////////////////////////////////////////
// avatar_task
//////////////////////////////////////////////////////////////////////////
avatar_task::avatar_task(
    int64_t _task_id,
    const std::shared_ptr<avatar_context>& _context,
    const std::shared_ptr<avatar_load_handlers>& _handlers)
    : context_(_context),
    handlers_(_handlers),
    task_id_(_task_id)
{
}

std::shared_ptr<avatar_context> avatar_task::get_context() const
{
    return context_;
}

std::shared_ptr<avatar_load_handlers> avatar_task::get_handlers() const
{
    return handlers_;
}

int64_t avatar_task::get_id() const
{
    return task_id_;
}




//////////////////////////////////////////////////////////////////////////
avatar_loader::avatar_loader()
    : task_id_(0)
    , working_(false)
    , network_error_(false)
    , local_thread_(std::make_shared<async_executer>("avl_local"))
    , server_thread_(std::make_shared<async_executer>("avl_server"))
{
}


avatar_loader::~avatar_loader()
{
}

using avatar_izes_array = std::map<int32_t, std::string_view>;

const avatar_izes_array& get_avatar_sizes()
{
    const static avatar_izes_array old_api_sizes = { {0, "ceilBigBuddyIcon"}, {64, "floorBigBuddyIcon"}, {128, "floorLargeBuddyIcon"} };
    const static avatar_izes_array new_api_sizes = { {0, "buddyIcon"}, {64, "bigBuddyIcon"}, {128, "largeBuddyIcon"} };

    return is_new_avatar_rapi() ? new_api_sizes : old_api_sizes;
}

std::string avatar_loader::get_avatar_type_by_size(int32_t _size) const
{
    for (const auto& [size, name] : boost::adaptors::reverse(get_avatar_sizes()))
        if (_size > size)
            return std::string(name);

    assert(false);

    return "buddyIcon";
}

std::wstring avatar_loader::get_avatar_path(const std::wstring& _avatars_data_path, std::string_view _contact, std::string_view _avatar_type) const
{
    std::wstring path = core::tools::from_utf8(_contact);
    std::replace(path.begin(), path.end(), L'|', L'_');

    path = _avatars_data_path + path;

    if (!_avatar_type.empty())
    {
        std::string lower_avatar_type = std::string(_avatar_type);
        std::transform(lower_avatar_type.begin(), lower_avatar_type.end(), lower_avatar_type.begin(), ::tolower);
        path += L'/' + core::tools::from_utf8(lower_avatar_type) + L"_.jpg";
    }

    return path;
}

bool load_avatar_from_file(std::shared_ptr<avatar_context> _context)
{
    if (!core::tools::system::is_exist(_context->avatar_file_path_))
        return false;

    boost::filesystem::wpath path(_context->avatar_file_path_);
    if (!_context->force_)
    {
        boost::system::error_code e;
        _context->write_time_ = last_write_time(path, e);
    }

    if (!_context->avatar_data_.load_from_file(_context->avatar_file_path_))
        return false;

    _context->avatar_exist_ = true;

    return true;
}


void avatar_loader::execute_task(std::shared_ptr<avatar_task> _task, std::function<void(int32_t)> _on_complete)
{
    time_t write_time = _task->get_context()->avatar_exist_ ? _task->get_context()->write_time_ : 0;

    if (!wim_params_)
    {
        assert(false);
        _on_complete(wim_protocol_internal_error::wpie_network_error);

        return;
    }

    auto packet = std::make_shared<request_avatar>(
        *wim_params_,
        _task->get_context()->contact_,
        _task->get_context()->avatar_type_,
        write_time);

    auto wr_this = weak_from_this();

    server_thread_->run_async_task(packet)->on_result_ = [wr_this, _task, packet, _on_complete](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == 0)
        {
            auto avatar_data = packet->get_data();

            ptr_this->local_thread_->run_async_function([avatar_data, _task]()->int32_t
            {
                uint32_t size = avatar_data->available();
                assert(size);
                if (size == 0)
                    return wpie_error_empty_avatar_data;

                _task->get_context()->avatar_data_.write(avatar_data->read(size), size);
                avatar_data->reset_out();

                avatar_data->save_2_file(_task->get_context()->avatar_file_path_);
                return 0;

            })->on_result_ = [avatar_data, wr_this, _on_complete, _task](int32_t _error)
            {
                auto ptr_this = wr_this.lock();
                if (!ptr_this)
                    return;

                if (_error == 0)
                {
                    if (_task->get_context()->avatar_exist_)
                        _task->get_handlers()->updated_(_task->get_context());
                    else
                        _task->get_handlers()->completed_(_task->get_context());
                }
                else
                {
                    _task->get_handlers()->failed_(_task->get_context(), _error);
                }

                _on_complete(_error);
            };
        }
        else
        {
            if (_task->get_context()->avatar_exist_)
            {
                long http_error = packet->get_http_code();
                if (http_error == 304) {}
            }
            else
            {
                _task->get_handlers()->failed_(_task->get_context(), _error);
            }

            _on_complete(_error);
        }
    };

}


void avatar_loader::run_tasks_loop()
{
    working_ = true;

    assert(!network_error_);

    auto task = get_next_task();
    if (!task)
    {
        working_ =  false;
        return;
    }

    auto wr_this = weak_from_this();


    execute_task(task, [wr_this, task](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == wim_protocol_internal_error::wpie_network_error)
        {
            ptr_this->network_error_ = true;
            ptr_this->working_ = false;

            return;
        }

        ptr_this->remove_task(task);
        ptr_this->run_tasks_loop();
    });
}


void avatar_loader::remove_task(std::shared_ptr<avatar_task> _task)
{
    requests_queue_.remove_if([_task](const std::shared_ptr<avatar_task>& _current_task)->bool
    {
        return (_current_task->get_id() == _task->get_id());
    });
}

void avatar_loader::add_task(std::shared_ptr<avatar_task> _task)
{
    requests_queue_.push_front(_task);
}

std::shared_ptr<avatar_task> avatar_loader::get_next_task()
{
    if (requests_queue_.empty())
    {
        return std::shared_ptr<avatar_task>();
    }

    return requests_queue_.front();
}

void avatar_loader::load_avatar_from_server(
    const std::shared_ptr<avatar_context>& _context,
    const std::shared_ptr<avatar_load_handlers>& _handlers)
{
    add_task(std::make_shared<avatar_task>(++task_id_, _context, _handlers));

    if (!working_ && !network_error_)
    {
        run_tasks_loop();
    }

    return;
}

std::shared_ptr<avatar_load_handlers> avatar_loader::get_contact_avatar_async(const wim_packet_params& _params, std::shared_ptr<avatar_context> _context)
{
    if (!wim_params_)
    {
        wim_params_ = std::make_shared<wim_packet_params>(_params);
    }
    else
    {
        *wim_params_ = _params;
    }

    auto handlers = std::make_shared<avatar_load_handlers>();

    _context->avatar_type_ = get_avatar_type_by_size(_context->avatar_size_);
    _context->avatar_file_path_ = get_avatar_path(_context->avatars_data_path_, _context->contact_, _context->avatar_type_);
    auto wr_this = weak_from_this();

    local_thread_->run_async_function([_context, handlers]()->int32_t
    {
        return (load_avatar_from_file(_context) ? 0 : -1);

    })->on_result_ = [wr_this, handlers, _context](int32_t _error)
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return;

        if (_error == 0)
        {
            handlers->completed_(_context);
        }

        ptr_this->load_avatar_from_server(_context, handlers);
    };

    return handlers;
}

std::shared_ptr<core::async_task_handlers> avatar_loader::remove_contact_avatars(const std::string& _contact, const std::wstring& _avatars_data_path)
{
    return local_thread_->run_async_function([_contact, _avatars_data_path, wr_this = weak_from_this()]()->int32_t
    {
        auto ptr_this = wr_this.lock();
        if (!ptr_this)
            return 0;

        const auto avatar_dir_path_ = ptr_this->get_avatar_path(_avatars_data_path, _contact);
        if (tools::system::is_exist(avatar_dir_path_))
            tools::system::clean_directory(avatar_dir_path_);

        return 0;
    });
}

void avatar_loader::resume(const wim_packet_params& _params)
{
    if (!wim_params_)
    {
        wim_params_ = std::make_shared<wim_packet_params>(_params);
    }
    else
    {
        *wim_params_ = _params;
    }

    if (!network_error_)
        return;

    network_error_ = false;

    assert(!working_);
    if (!working_)
    {
        run_tasks_loop();
    }
}

void avatar_loader::show_contact_avatar(const std::string& _contact, const int32_t _avatar_size)
{
    for (auto iter = requests_queue_.begin(); iter != requests_queue_.end(); ++iter)
    {
        if (const auto ctx = (*iter)->get_context(); ctx->avatar_size_ == _avatar_size && ctx->contact_ == _contact)
        {
            auto task = *iter;

            requests_queue_.erase(iter);

            requests_queue_.push_back(std::move(task));

            break;
        }
    }
}
