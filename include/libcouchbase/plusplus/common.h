#ifndef LCB_CPP_COMMON_H
#define LCB_CPP_COMMON_H

#include <libcouchbase/couchbase.h>
namespace Couchbase {

typedef lcb_get_cmd_t C_GetCmd;
typedef lcb_touch_cmd_t C_TouchCmd;
typedef lcb_store_cmd_t C_StoreCmd;
typedef lcb_remove_cmd_t C_RemoveCmd;
typedef lcb_arithmetic_cmd_t C_ArithCmd;
typedef lcb_unlock_cmd_t C_UnlockCmd;
typedef lcb_observe_cmd_t C_ObserveCmd;
typedef lcb_durability_cmd_t C_DurabilityCmd;

typedef lcb_get_resp_t C_GetResp;
typedef lcb_touch_resp_t C_TouchResp;
typedef lcb_store_resp_t C_StoreResp;
typedef lcb_arithmetic_resp_t C_ArithResp;
typedef lcb_unlock_resp_t C_UnlockResp;
typedef lcb_remove_resp_t C_RemoveResp;

class Connection;
class ResponseHandler;
class OperationBase;
class OperationContext;
class Command;
class KeyCommand;

}

#endif
