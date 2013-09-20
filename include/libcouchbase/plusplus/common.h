#ifndef LCB_CPP_COMMON_H
#define LCB_CPP_COMMON_H

#include <libcouchbase/couchbase.h>
namespace Couchbase {

typedef lcb_get_cmd_t LcbGetCommand;
typedef lcb_touch_cmd_t LcbTouchCommand;
typedef lcb_store_cmd_t LcbStoreCommand;
typedef lcb_remove_cmd_t LcbDeleteCommand;
typedef lcb_arithmetic_cmd_t LcbArithmeticCommand;
typedef lcb_unlock_cmd_t LcbUnlockCommand;
typedef lcb_observe_cmd_t LcbObserveCommand;
typedef lcb_durability_cmd_t LcbDurabilityCommand;

typedef lcb_get_resp_t LcbGetResponse;
typedef lcb_touch_resp_t LcbTouchResponse;
typedef lcb_store_resp_t LcbStoreResponse;
typedef lcb_arithmetic_resp_t LcbArithmeticResponse;
typedef lcb_unlock_resp_t LcbUnlockResponse;
typedef lcb_remove_resp_t LcbDeleteResponse;

class Connection;
class ResponseHandler;
class OperationBase;
class OperationContext;
class Command;
class KeyCommand;

}

#endif
