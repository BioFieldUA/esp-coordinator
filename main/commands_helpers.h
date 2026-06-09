typedef uint16_t __attribute__((aligned(1))) unalign_uint16_t;
typedef uint32_t __attribute__((aligned(1))) unalign_uint32_t;

/** @struct commonResponse
 */
struct generic_response_t {
    ncp_status_category_t category;
    ncp_generic_status_t status;
} __attribute__((packed)) __attribute__((aligned(1)));

struct zdo_device_annce_params_t {
    zb_uint16_t dev_short_addr;
    zb_ieee_addr_t dev_ieee;
    zb_uint8_t capability;
} __attribute__((packed)) __attribute__((aligned(1)));

struct zdo_device_leave_params_t {
    zb_ieee_addr_t device_ieee;
    zb_uint8_t rejoin;
} __attribute__((packed)) __attribute__((aligned(1)));

/**
 * @name Standard Response Handlers
 * @brief Templates for generating unified responses with generic status and optional payload.
 */
template <command_id_t CmdId, typename Res> struct zb_ncp::general_status_res {
    struct FullRes {
        generic_response_t status;
        Res res;
    } __attribute__((packed)) __attribute__((aligned(1)));
    static constexpr uint16_t resp_buffer_size = sizeof(FullRes);
    static uint16_t process_immediate(const void* inbuffer, uint16_t inlen, uint8_t* outdata, uint16_t outlen) {
        auto full_res = reinterpret_cast<FullRes*>(outdata);
        full_res->status.category = STATUS_CATEGORY_GENERIC;
        full_res->status.status = GENERIC_OK;
        zb_ncp::cmd_handle<CmdId>::process_status_res(full_res->status.status, &full_res->res);
        return sizeof(FullRes);
    }
};

template <command_id_t CmdId, typename Arg> struct zb_ncp::general_status_arg {
    static constexpr uint16_t resp_buffer_size = sizeof(generic_response_t);
    static uint16_t process_immediate(const void* inbuffer, uint16_t inlen, uint8_t* outdata, uint16_t outlen) {
        auto full_res = reinterpret_cast<generic_response_t*>(outdata);
        full_res->category = STATUS_CATEGORY_GENERIC;
        full_res->status = GENERIC_OK;
        if (inlen < sizeof(Arg)) {
            full_res->status = GENERIC_INVALID_PARAMETER;
            return sizeof(generic_response_t);
        }
        zb_ncp::cmd_handle<CmdId>::process_status_arg(full_res->status, *static_cast<const Arg*>(inbuffer));
        return sizeof(generic_response_t);
    }
};

template <command_id_t CmdId, typename Arg, typename Res> struct zb_ncp::general_status_arg_res {
    struct FullRes {
        generic_response_t status;
        Res res;
    } __attribute__((packed)) __attribute__((aligned(1)));
    static constexpr uint16_t resp_buffer_size = sizeof(FullRes);
    static uint16_t process_immediate(const void* inbuffer, uint16_t inlen, uint8_t* outdata, uint16_t outlen) {
        auto full_res = reinterpret_cast<FullRes*>(outdata);
        full_res->status.category = STATUS_CATEGORY_GENERIC;
        full_res->status.status = GENERIC_OK;
        if (inlen < sizeof(Arg)) {
            full_res->status.status = GENERIC_INVALID_PARAMETER;
            return sizeof(FullRes);
        }
        zb_ncp::cmd_handle<CmdId>::process_status_arg_res(full_res->status.status, *static_cast<const Arg*>(inbuffer), &full_res->res);
        return sizeof(FullRes);
    }
};

/**
 * @name Synchronous Execution Strategies
 * @brief Logic for immediate command dispatching and response routing.
 */
template <command_id_t CmdId> struct zb_ncp::cmd_handle : 
    immediate_cmd_process<CmdId> {
    static constexpr uint16_t resp_buffer_size = sizeof(generic_response_t);
    static uint16_t process_immediate(const void* inbuffer, uint16_t inlen, uint8_t* outdata, uint16_t outlen) {
        auto full_res = reinterpret_cast<generic_response_t*>(outdata);
        full_res->category = STATUS_CATEGORY_GENERIC;
        full_res->status = GENERIC_NOT_IMPLEMENTED;
        ESP_LOGE(TAG, "Command %s is not implemented yet", cmd_to_str(CmdId));
        return sizeof(generic_response_t);
    }
};

template <command_id_t CmdId> struct zb_ncp::immediate_cmd_process {
    using Cmd = cmd_handle<CmdId>;
    static void process(const zb_ncp::cmd_t& cmd, const void* buffer, uint16_t len) {
        uint8_t outdata[sizeof(zb_ncp::cmd_t) + Cmd::resp_buffer_size];
        auto outlen = sizeof(zb_ncp::cmd_t) + Cmd::process_immediate(buffer, len, &outdata[sizeof(zb_ncp::cmd_t)], Cmd::resp_buffer_size);
        zb_ncp::cmd_t* out_cmd = reinterpret_cast<zb_ncp::cmd_t*>(outdata);
        *out_cmd = cmd;
        out_cmd->type = RESPONSE;
        zb_ncp::send_cmd_data(outdata, outlen);
    }
};

/**
 * @name Asynchronous Execution Strategies
 * @brief Handles commands that require waiting for a Zigbee stack callback.
 */
template <command_id_t CmdId> struct single_cmd_delayed {
    static zb_ncp::cmd_t m_cmd;
    static void start_resolve(const zb_ncp::cmd_t& cmd) {
        m_cmd = cmd;
    }
    static bool need_resolve() {
        return m_cmd.command_id == CmdId;
    }
    static void resolve(zb_ncp::cmd_t& cmd) {
        cmd = m_cmd;
        m_cmd.command_id = command_id_t(0);
    }
};

#define SINGLE_CMD_DELAYED_DECL(CmdId) template <> zb_ncp::cmd_t single_cmd_delayed<CmdId>::m_cmd = {};

template <command_id_t CmdId, template<command_id_t> typename ResolveStrategyT> struct zb_ncp::delayed_cmd_process :
    public ResolveStrategyT<CmdId> {
    using Cmd = cmd_handle<CmdId>;
    using ResolveStrategy = ResolveStrategyT<CmdId>;
    static void process(const zb_ncp::cmd_t& cmd, const void* buffer, uint16_t len) {
        ResolveStrategy::start_resolve(cmd);
        ncp_generic_status_t res = Cmd::start_delayed(buffer, len);
        if (res != GENERIC_OK) {
            ESP_LOGE(TAG, "%s::start_delayed failed. Error: %s (%d)", Cmd::name, utils::get_generic_status_str(res), res);
            response(res);
        }
    }
    static bool response(ncp_generic_status_t status) {
        if (ResolveStrategy::need_resolve()) {
            uint8_t outdata[sizeof(zb_ncp::cmd_t) + Cmd::resp_buffer_size];
            zb_ncp::cmd_t* out_cmd = reinterpret_cast<zb_ncp::cmd_t*>(outdata);
            ResolveStrategy::resolve(*out_cmd);
            out_cmd->type = RESPONSE;
            auto outlen = sizeof(zb_ncp::cmd_t) + Cmd::finish_delayed(status, &outdata[sizeof(zb_ncp::cmd_t)], Cmd::resp_buffer_size);
            zb_ncp::send_cmd_data(outdata, outlen);
            return true;
        }
        return false;
    }
};

/**
 * @name Complex Request-Response Handlers
 * @brief Manages asynchronous Zigbee transactions (ZDO/ZCL) with TSN tracking and retries.
 */
template <typename Cmd> struct cmd_base {
    static constexpr ncp_status_category_t status_category = STATUS_CATEGORY_GENERIC;
    static void report_status(uint8_t status, generic_response_t& resp) {
        resp.category = Cmd::status_category;
        resp.status = static_cast<ncp_generic_status_t>(status);
        if (status != GENERIC_OK) {
            ESP_LOGE(TAG, "%s::report_status: %s (%d)", Cmd::name, utils::get_zdp_status_str(status), status);
        }
    }
    static void report_failed(const zb_ncp::cmd_t& src_cmd, uint8_t status) {
        uint8_t outdata[sizeof(zb_ncp::cmd_t) + sizeof(generic_response_t)];
        zb_ncp::cmd_t* out_cmd = reinterpret_cast<zb_ncp::cmd_t*>(outdata);
        *out_cmd = src_cmd;
        out_cmd->type = zb_ncp::RESPONSE;
        report_status(status, *reinterpret_cast<generic_response_t*>(out_cmd + 1));
        zb_ncp::send_cmd_data(outdata, sizeof(outdata));
    }
};

template <command_id_t CmdId, typename ArgType> struct request_cmd_resolver {
    using Arg = ArgType;
    enum state_t : uint8_t { S_NONE, S_ALLOCATION, S_EXEC };
    struct request_t {
        zb_ncp::cmd_t cmd;
        uint8_t tsn;
        uint16_t alloc_len;
        uint16_t old;
        std::atomic<state_t> state{ state_t::S_NONE };
        Arg arg;
    };
    using requests_arr_t = request_t[zb_ncp::MAX_PARALLEL_REQUESTS];
    static requests_arr_t& storage() {
        static requests_arr_t s_requests;
        return s_requests;
    }
    static std::atomic<uint16_t>& old_cntr() {
        static std::atomic<uint16_t> s{ 0 };
        return s;
    }
    static request_t* start_resolve(const zb_ncp::cmd_t& cmd, uint8_t* req_idx) {
        assert(req_idx != nullptr);
        while (true) {
            for (uint8_t i = 0; i < zb_ncp::MAX_PARALLEL_REQUESTS; ++i) {
                auto& req = storage()[i];
                state_t expected = state_t::S_NONE;
                if (req.state.compare_exchange_strong(expected, state_t::S_ALLOCATION, std::memory_order_acquire)) {
                    *req_idx = i;
                    req.cmd = cmd;
                    req.tsn = 0;
                    req.alloc_len = 0;
                    req.old = old_cntr().fetch_add(1, std::memory_order_acq_rel);
                    return &req;
                }
            }
            uint16_t cur_old = old_cntr().load(std::memory_order_acquire);
            uint16_t max_dist = 0;
            request_t* candidate = nullptr;
            uint8_t candidate_idx = 0;
            for (uint8_t i = 0; i < zb_ncp::MAX_PARALLEL_REQUESTS; ++i) {
                auto& req = storage()[i];
                if (req.state.load(std::memory_order_acquire) == state_t::S_EXEC) {
                    uint16_t dist = static_cast<uint16_t>(cur_old - req.old);
                    if (dist >= max_dist) {
                        max_dist = dist;
                        candidate_idx = i;
                        candidate = &req;
                    }
                }
            }
            if (candidate) {
                state_t expected = state_t::S_EXEC;
                if (candidate->state.compare_exchange_strong(expected, state_t::S_ALLOCATION, std::memory_order_acquire)) {
                    ESP_LOGW(TAG, "Override request with TSN: %d (distance: %d)", candidate->tsn, max_dist);
                    *req_idx = candidate_idx;
                    candidate->cmd = cmd;
                    candidate->tsn = 0;
                    candidate->alloc_len = 0;
                    candidate->old = old_cntr().fetch_add(1, std::memory_order_acq_rel);
                    return candidate;
                }
            }
            if (xPortInIsrContext() == pdTRUE) {
                portYIELD_FROM_ISR(pdTRUE);
            } else {
                portYIELD();
            }
        }
    }
    static request_t* resolve(uint8_t tsn) {
        for (auto& req : storage()) {
            if ((req.state.load(std::memory_order_acquire) == state_t::S_EXEC) && (req.tsn == tsn)) {
                return &req;
            }
        }
        return nullptr;
    }
    static request_t& get_by_index(uint8_t idx) {
        return storage()[idx];
    }
};

template <typename Resp> struct resp_parser {
    static inline uint8_t get_status(const Resp* resp) {
        return resp->status;
    }
    static inline uint8_t get_tsn(const Resp* resp) {
        return resp->tsn;
    }
};

template <> struct resp_parser<zb_zdo_simple_desc_resp_t> {
    static inline uint8_t get_status(const zb_zdo_simple_desc_resp_t* resp) {
        return resp->hdr.status;
    }
    static inline uint8_t get_tsn(const zb_zdo_simple_desc_resp_t* resp) {
        return resp->hdr.tsn;
    }
};

template <> struct resp_parser<zb_zdo_node_desc_resp_t> {
    static inline uint8_t get_status(const zb_zdo_node_desc_resp_t* resp) {
        return resp->hdr.status;
    }
    static inline uint8_t get_tsn(const zb_zdo_node_desc_resp_t* resp) {
        return resp->hdr.tsn;
    }
};

template <> struct resp_parser<zb_zdo_power_desc_resp_t> {
    static inline uint8_t get_status(const zb_zdo_power_desc_resp_t* resp) {
        return resp->hdr.status;
    }
    static inline uint8_t get_tsn(const zb_zdo_power_desc_resp_t* resp) {
        return resp->hdr.tsn;
    }
};

template <command_id_t CmdId, typename Arg, typename Req, typename Resp> struct zb_ncp::request_cmd_process : 
    public request_cmd_resolver<CmdId, Arg> {
    enum alloc_t : uint8_t {
        INITIAL = 0,
        TAIL = 1,
        PARAM = 2
    };
    using Cmd = zb_ncp::cmd_handle<CmdId>;
    using ArgType = Arg;
    using ResolveStrategy = request_cmd_resolver<CmdId, Arg>;
    static constexpr const alloc_t alloc_request = alloc_t::INITIAL;
    static constexpr const char* name = "";
    static constexpr ncp_status_category_t status_category = STATUS_CATEGORY_ZDO;
    static void report_status(uint8_t status, generic_response_t& resp) {
        cmd_base<Cmd>::report_status(status, resp);
    }
    static void report_failed(const zb_ncp::cmd_t& src_cmd, uint8_t status) {
        cmd_base<Cmd>::report_failed(src_cmd, status);
    }
    static uint16_t get_outdata_len(const Resp* resp, uint16_t buf_len) {
        return sizeof(Resp);
    }
    static ncp_generic_status_t get_response(const Resp* resp, uint8_t* outdata, uint16_t outlen) {
        memcpy(outdata, resp, outlen);
        return GENERIC_OK;
    }
    static void send_response(ResolveStrategy::request_t& req, const Resp* resp, uint16_t buf_len) {
        uint16_t len = Cmd::get_outdata_len(resp, buf_len);
        uint8_t outdata[sizeof(zb_ncp::cmd_t) + sizeof(generic_response_t) + len];
        zb_ncp::cmd_t* out_cmd = reinterpret_cast<zb_ncp::cmd_t*>(outdata);
        *out_cmd = req.cmd;
        out_cmd->type = zb_ncp::RESPONSE;
        generic_response_t* out_status = reinterpret_cast<generic_response_t*>(out_cmd + 1);
        ncp_generic_status_t status = Cmd::get_response(resp, reinterpret_cast<uint8_t*>(out_status + 1), len);
        report_status(status, *out_status);
        zb_ncp::send_cmd_data(outdata, sizeof(outdata));
    }
    static void req_callback(uint8_t buf) {
        if (buf) {
            uint16_t buf_len = zb_buf_len(buf);
            auto resp = reinterpret_cast<const Resp*>(zb_buf_begin(buf));
            auto tsn = resp_parser<Resp>::get_tsn(resp);
            auto req = ResolveStrategy::resolve(tsn);
            if (req) {
                ESP_LOGD(TAG, "%s::req_callback TSN: %d", Cmd::name, tsn);
                auto status = resp_parser<Resp>::get_status(resp);
                if (status == ZB_ZDP_STATUS_SUCCESS) {
                    Cmd::send_response(*req, resp, buf_len);
                } else {
                    report_failed(req->cmd, status);
                }
                req->state.store(ResolveStrategy::state_t::S_NONE, std::memory_order_release);
            } else {
                ESP_LOGE(TAG, "%s::req_callback Request with TSN=%d not found", Cmd::name, tsn);
            }
            zb_buf_free(buf);
        } else {
            ESP_LOGE(TAG, "%s::req_callback buffer failed", Cmd::name);
        }
    }
    static void format_request(Req& req, const Arg& arg) {
        req = arg;
    }
    static uint16_t get_request_alloc_size(const Arg& arg) {
        return sizeof(Req);
    }
    static void do_request(uint8_t buf, uint16_t req_idx) {
        auto& req = ResolveStrategy::get_by_index((uint8_t)req_idx);
        if (!buf) {
            ESP_LOGE(TAG, "%s::do_request failed (error: no buffer memory)", Cmd::name);
            zb_buf_free(buf);
            report_failed(req.cmd, ZB_ZDP_STATUS_INSUFFICIENT_SPACE);
            req.state.store(ResolveStrategy::state_t::S_NONE, std::memory_order_release);
            return;
        }
        Req* request_data;
        switch (Cmd::alloc_request) {
        case alloc_t::INITIAL:
            request_data = static_cast<Req*>(zb_buf_initial_alloc(buf, req.alloc_len));
            break;
        case alloc_t::TAIL:
            request_data = static_cast<Req*>(zb_buf_alloc_tail(buf, req.alloc_len));
            break;
        case alloc_t::PARAM:
            request_data = ZB_BUF_GET_PARAM(buf, Req);
            break;
        }
        if (!request_data) {
            zb_buf_free(buf);
            report_failed(req.cmd, ZB_ZDP_STATUS_TABLE_FULL);
            req.state.store(ResolveStrategy::state_t::S_NONE, std::memory_order_release);
            return;
        }
        Cmd::format_request(*request_data, req.arg);
        auto r = Cmd::start_request(buf);
        if (r == 0xFF) {
            zb_buf_free(buf);
            report_failed(req.cmd, ZB_ZDP_STATUS_INSUFFICIENT_SPACE);
            req.state.store(ResolveStrategy::state_t::S_NONE, std::memory_order_release);
            return;
        }
        req.tsn = r;
        req.state.store(ResolveStrategy::state_t::S_EXEC, std::memory_order_release);
        ESP_LOGD(TAG, "%s::do_request req_idx: %d, TSN: %d", Cmd::name, req_idx, r);
    }
    static void buf_alloc(uint8_t req_idx) {
        auto& req = ResolveStrategy::get_by_index(req_idx);
        auto ret = zb_buf_get_out_delayed_ext(Cmd::do_request, req_idx, req.alloc_len);
        if (ret != RET_OK) {
            report_failed(req.cmd, ZB_ZDP_STATUS_INSUFFICIENT_SPACE);
            req.state.store(ResolveStrategy::state_t::S_NONE, std::memory_order_release);
            return;
        }
    }
    static bool check_arg_size(const void* buffer, uint16_t len) {
        return len == sizeof(Arg);
    }
    static void process(const zb_ncp::cmd_t& cmd, const void* buffer, uint16_t len) {
        if (!Cmd::check_arg_size(buffer, len)) {
            report_failed(cmd, ZB_ZDP_STATUS_INVALID_INDEX);
            return;
        }
        uint8_t req_idx;
        auto req = ResolveStrategy::start_resolve(cmd, &req_idx);
        if (!req) {
            report_failed(cmd, ZB_ZDP_STATUS_INSUFFICIENT_SPACE);
            return;
        }
        req->arg = *reinterpret_cast<const Arg*>(buffer);
        req->alloc_len = Cmd::get_request_alloc_size(req->arg);
        zb_ret_t ret;
        {
            utils::critical_section lock(&zb_ncp::m_mux_lock);
            ret = ZB_SCHEDULE_APP_CALLBACK(Cmd::buf_alloc, req_idx);
        }
        if (ret != RET_OK) {
            report_failed(cmd, ZB_ZDP_STATUS_INSUFFICIENT_SPACE);
            req->state.store(ResolveStrategy::state_t::S_NONE, std::memory_order_release);
            return;
        }
        ESP_LOGD(TAG, "%s::do_start req_idx: %d", Cmd::name, req_idx);
    }
};
