#include "stdafx.h"
#include "get_chat_info.h"

#include "../../../http_request.h"

#include "../../urls_cache.h"

using namespace core;
using namespace wim;

constexpr int32_t max_members_count = 150;

get_chat_info::get_chat_info(wim_packet_params _params, get_chat_info_params _chat_params)
    :   robusto_packet(std::move(_params)),
        params_(std::move(_chat_params))
{
}


get_chat_info::~get_chat_info()
{

}

int32_t get_chat_info::init_request(std::shared_ptr<core::http_request_simple> _request)
{
    constexpr char method[] = "getChatInfo";

    _request->set_gzip(true);
    _request->set_url(urls::get_url(urls::url_type::rapi_host));
    _request->set_normalized_url(method);
    _request->set_keep_alive();

    rapidjson::Document doc(rapidjson::Type::kObjectType);
    auto& a = doc.GetAllocator();

    doc.AddMember("method", method, a);
    doc.AddMember("reqId", get_req_id(), a);

    rapidjson::Value node_params(rapidjson::Type::kObjectType);

    if (!params_.aimid_.empty())
    {
        node_params.AddMember("sn", params_.aimid_, a);
    }
    else if (!params_.stamp_.empty())
    {
        node_params.AddMember("stamp", params_.stamp_, a);
    }

    node_params.AddMember("memberLimit", params_.members_limit_ ? params_.members_limit_ : max_members_count, a);

    doc.AddMember("params", std::move(node_params), a);

    sign_packet(doc, a, _request);

    if (!robusto_packet::params_.full_log_)
    {
        log_replace_functor f;
        f.add_marker("a");
        f.add_json_marker("aimsid", aimsid_range_evaluator());
        _request->set_replace_log_function(f);
    }

    return 0;
}

int32_t get_chat_info::parse_results(const rapidjson::Value& _node_results)
{
    if (result_.unserialize(_node_results) != 0)
        return wpie_http_parse_response;

    return 0;
}

int32_t get_chat_info::on_response_error_code()
{
    if (status_code_ == 40001)
        return wpie_error_robusto_you_are_not_chat_member;
    else if (status_code_ == 40002)
        return wpie_error_robusto_you_are_blocked;

    return robusto_packet::on_response_error_code();
}
