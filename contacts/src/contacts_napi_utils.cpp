/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "contacts_napi_utils.h"
#include "hilog_wrapper_api.h"
#include "ability.h"
#include "ability_context.h"
#include "context.h"
#include "ui_content.h"
#include "ui_extension_context.h"
#include "want.h"
#include "contact_picker_protocol.h"
#include "int_wrapper.h"
#include "string_wrapper.h"
#include "modal_ui_extension_config.h"
#include <uv.h>

namespace OHOS {
namespace ContactsApi {
static constexpr const char *JS_ERROR_INVALID_INPUT_PARAMETER_STRING =
    "parameter error. Mandatory parameters are left unspecified.";
static constexpr const char *JS_ERROR_VERIFICATION_FAILED_PARAMETER_STRING =
    "parameter error. The type of id must be number.";
static constexpr const char *JS_ERROR_INVALID_PARAMETER_STRING =
    "parameter error. Invalid parameter value.";
static constexpr const char *JS_ERROR_PERMISSION_DENIED_STRING = "Permission denied";
napi_value ContactsNapiUtils::ToInt32Value(napi_env env, int32_t value)
{
    napi_value staticValue = nullptr;
    napi_create_int32(env, value, &staticValue);
    return staticValue;
}

napi_value ContactsNapiUtils::CreateClassConstructor(napi_env env, napi_callback_info info)
{
    napi_value thisArg = nullptr;
    void *data = nullptr;
    napi_get_cb_info(env, info, nullptr, nullptr, &thisArg, &data);
    napi_value global = nullptr;
    napi_get_global(env, &global);
    return thisArg;
}

bool ContactsNapiUtils::MatchValueType(napi_env env, napi_value value, napi_valuetype targetType)
{
    napi_valuetype valueType = napi_undefined;
    napi_typeof(env, value, &valueType);
    return valueType == targetType;
}

bool ContactsNapiUtils::MatchParameters(
    napi_env env, const napi_value parameters[], std::initializer_list<napi_valuetype> valueTypes)
{
    if (parameters == nullptr) {
        return false;
    }
    int i = 0;
    for (auto beg = valueTypes.begin(); beg != valueTypes.end(); ++beg) {
        if (!MatchValueType(env, parameters[i], *beg)) {
            return false;
        }
        ++i;
    }
    return true;
}

napi_value ContactsNapiUtils::CreateError(napi_env env, int32_t err)
{
    napi_value businessError = nullptr;
    napi_value errorCode = nullptr;
    napi_value errorMessage = nullptr;
    if (err == PERMISSION_ERROR) {
        napi_create_string_utf8(env, JS_ERROR_PERMISSION_DENIED_STRING, NAPI_AUTO_LENGTH, &errorMessage);
    } else if (err == PARAMETER_ERROR) {
        napi_create_string_utf8(env, JS_ERROR_INVALID_INPUT_PARAMETER_STRING, NAPI_AUTO_LENGTH, &errorMessage);
    } else if (err == INVALID_PARAMETER) {
        napi_create_string_utf8(env, JS_ERROR_INVALID_PARAMETER_STRING, NAPI_AUTO_LENGTH, &errorMessage);
    }
    napi_create_int32(env, err, &errorCode);
    napi_create_error(env, nullptr, errorMessage, &businessError);
    napi_set_named_property(env, businessError, "code", errorCode);
    HILOG_INFO("CreateError end");
    return businessError;
}

napi_value ContactsNapiUtils::CreateErrorByVerification(napi_env env, int32_t err)
{
    napi_value businessError = nullptr;
    napi_value errorCode = nullptr;
    napi_value errorMessage = nullptr;
    if (err == PERMISSION_ERROR) {
        napi_create_string_utf8(env, JS_ERROR_PERMISSION_DENIED_STRING, NAPI_AUTO_LENGTH, &errorMessage);
    } else if (err == PARAMETER_ERROR) {
        napi_create_string_utf8(env, JS_ERROR_VERIFICATION_FAILED_PARAMETER_STRING, NAPI_AUTO_LENGTH, &errorMessage);
    } else if (err == INVALID_PARAMETER) {
        napi_create_string_utf8(env, JS_ERROR_INVALID_PARAMETER_STRING, NAPI_AUTO_LENGTH, &errorMessage);
    }
    napi_create_int32(env, err, &errorCode);
    napi_create_error(env, nullptr, errorMessage, &businessError);
    napi_set_named_property(env, businessError, "code", errorCode);
    HILOG_INFO("CreateErrorByVerification end");
    return businessError;
}

OHOS::Ace::UIContent* ContactsNapiUtils::GetUIContent(
    std::unique_ptr<OHOS::ContactsApi::BaseContext>& asyncContext)
{
    OHOS::Ace::UIContent* uiContent = nullptr;
    if (asyncContext->context != nullptr) {
        HILOG_INFO("get uiContext by ability context");
        uiContent = asyncContext->context->GetUIContent();
    } else if (asyncContext->uiExtensionContext != nullptr) {
        HILOG_INFO("get uiContext by uiExtensionContext");
        uiContent = asyncContext->uiExtensionContext->GetUIContent();
    } else {
        HILOG_ERROR("get uiContext failed.");
    }
    HILOG_INFO("ContactsNapiUtils::GetUIContent END");
    return uiContent;
}

napi_status ContactsNapiUtils::GetParamCallback(napi_env env,
    std::unique_ptr<OHOS::ContactsApi::BaseContext> &context, size_t argc, napi_value argv[])
{
    /* Parse the last argument into callbackref if any */
    bool isCallback = false;
    CHECK_STATUS_RET(HasCallback(env, argc, argv, isCallback), "Failed to check callback");
    if (isCallback) {
        CHECK_STATUS_RET(GetParamFunction(env, argv[argc - 1], context->callbackRef),
            "Failed to get callback");
    }
    return napi_ok;
}

void ContactsNapiUtils::InvokeJSAsyncMethod(napi_env env, napi_deferred deferred, napi_ref callbackRef,
    napi_async_work work, const JSAsyncContextOutput &asyncContext)
{
    napi_value retVal;
    napi_value callback = nullptr;

    /* Deferred is used when JS Callback method expects a promise value */
    if (deferred) {
        if (asyncContext.status) {
            napi_resolve_deferred(env, deferred, asyncContext.data);
        } else {
            napi_reject_deferred(env, deferred, asyncContext.error);
        }
    } else {
        napi_value result[ARGS_TWO];
        result[PARAM0] = asyncContext.error;
        result[PARAM1] = asyncContext.data;
        napi_get_reference_value(env, callbackRef, &callback);
        napi_call_function(env, nullptr, callback, ARGS_TWO, result, &retVal);
        napi_delete_reference(env, callbackRef);
    }
    napi_delete_async_work(env, work);
}

napi_value ContactsNapiUtils::NapiCreateAsyncWork(napi_env env,
    std::unique_ptr<ContactsApi::BaseContext> &asyncContext, const std::string &resourceName,
    void (*execute)(napi_env, void *), void (*complete)(napi_env, napi_status, void *))
{
    napi_value result = nullptr;
    napi_value resource = nullptr;
    NAPI_CREATE_PROMISE(env, asyncContext->callbackRef, asyncContext->deferred, result);
    NAPI_CREATE_RESOURCE_NAME(env, resource, resourceName.c_str());

    NAPI_CALL(env, napi_create_async_work(env, nullptr, resource, execute, complete,
        static_cast<void *>(asyncContext.get()), &asyncContext->work));
    NAPI_CALL(env, napi_queue_async_work(env, asyncContext->work));
    asyncContext.release();

    return result;
}

napi_status ContactsNapiUtils::HasCallback(napi_env env, const size_t argc, const napi_value argv[],
    bool &isCallback)
{
    isCallback = false;
    if (argc < ARGS_ONE) {
        return napi_ok;
    }
    napi_valuetype valueType = napi_undefined;
    napi_status statusRet = napi_typeof(env, argv[argc - 1], &valueType);
    if (statusRet != napi_ok) {
        return statusRet;
    }
    isCallback = (valueType == napi_function);
    return napi_ok;
}

napi_status ContactsNapiUtils::GetParamFunction(napi_env env, napi_value arg, napi_ref &callbackRef)
{
    napi_valuetype valueType = napi_undefined;
    napi_status statusRet = napi_typeof(env, arg, &valueType);
    if (statusRet != napi_ok) {
        return statusRet;
    }
    CHECK_COND_RET(valueType == napi_function, napi_function_expected, "Type is not as expected function");
    statusRet = napi_create_reference(env, arg, NAPI_INIT_REF_COUNT, &callbackRef);
    if (statusRet != napi_ok) {
        return statusRet;
    }
    return napi_ok;
}

void ContactsNapiUtils::ThrowError(napi_env env, int32_t err, const std::string &errMsg)
{
    std::string message = errMsg;
    if (message.empty()) {
        message = "operation not support";
    }

    HILOG_ERROR("ThrowError errCode:%{public}d errMsg:%{public}s", err, message.c_str());
    NAPI_CALL_RETURN_VOID(env, napi_throw_error(env, std::to_string(err).c_str(), message.c_str()));
}

void ContactsNapiUtils::ThrowError(napi_env env, int32_t err,
    const char *funcName, int32_t line, const std::string &errMsg)
{
    std::string message = errMsg;
    if (message.empty()) {
        message = "operation not support";
    }
    HILOG_ERROR("{%{public}s:%d} ThrowError errCode:%{public}d errMsg:%{public}s", funcName, line,
        err, message.c_str());
    NAPI_CALL_RETURN_VOID(env, napi_throw_error(env, std::to_string(err).c_str(), message.c_str()));
}

namespace {
struct SelectionRequest {
    std::mutex mutex;
    ContactPickerProtocol protocol;
    napi_env env = nullptr;
    napi_deferred deferred = nullptr;
    napi_ref callback = nullptr;
    std::weak_ptr<AbilityRuntime::AbilityContext> ability;
    std::weak_ptr<AbilityRuntime::UIExtensionContext> extension;
    Ace::UIContent *content = nullptr; // identity only; never dereference after onDestroy
    int32_t session = 0;
    bool alive = true;
    bool settled = false;
    bool closed = false;
    uv_async_t *delivery = nullptr;
    uv_timer_t *deadline = nullptr;
    std::weak_ptr<SelectionRequest> *cleanup = nullptr;
};
using Selection = std::shared_ptr<SelectionRequest>;
constexpr uint64_t SELECTION_TIMEOUT_MS = 10 * 60 * 1000;

void CloseSelection(const Selection &state)
{
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->closed || state->content == nullptr || state->session <= 0) { return; }
    state->closed = true;
    auto ability = state->ability.lock();
    auto extension = state->extension.lock();
    auto content = ability != nullptr ? ability->GetUIContent() :
        (extension != nullptr ? extension->GetUIContent() : nullptr);
    if (content != nullptr && content == state->content) {
        // UIContentImpl posts a native session-id task. onDestroy invalidates
        // this identity under the same lock before any later close can use it.
        content->CloseModalUIExtension(state->session);
    }
}

void CloseSelectionHandles(const Selection &state)
{
    // Only invoked on the env's loop (completion or cleanup), never from UI.
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->deadline != nullptr) {
        uv_timer_stop(state->deadline);
        uv_close(reinterpret_cast<uv_handle_t *>(state->deadline), [](uv_handle_t *handle) {
            delete static_cast<Selection *>(handle->data);
            delete reinterpret_cast<uv_timer_t *>(handle);
        });
        state->deadline = nullptr;
    }
    if (state->delivery != nullptr) {
        uv_close(reinterpret_cast<uv_handle_t *>(state->delivery), [](uv_handle_t *handle) {
            delete static_cast<Selection *>(handle->data);
            delete reinterpret_cast<uv_async_t *>(handle);
        });
        state->delivery = nullptr;
    }
}

void CleanupSelection(void *data)
{
    std::unique_ptr<std::weak_ptr<SelectionRequest>> cleanup(static_cast<std::weak_ptr<SelectionRequest> *>(data));
    auto state = cleanup->lock();
    if (state == nullptr) { return; }
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->alive = false;
        state->env = nullptr;
        state->deferred = nullptr;
        state->callback = nullptr; // env now owns disposal of its references
        state->cleanup = nullptr;
    }
    state->protocol.Fail();
    CloseSelection(state);
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->content = nullptr;
        state->closed = true;
    }
    CloseSelectionHandles(state);
}

void CompleteSelection(const Selection &state)
{
    const auto result = state->protocol.Snapshot();
    if (!result.ready) { return; }
    napi_env env;
    napi_deferred deferred;
    napi_ref callback;
    std::weak_ptr<SelectionRequest> *cleanup;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (!state->alive || state->settled) { return; }
        state->settled = true;
        env = state->env;
        deferred = state->deferred;
        callback = state->callback;
        cleanup = state->cleanup;
        state->deferred = nullptr;
        state->callback = nullptr;
    }
    CloseSelectionHandles(state);
    if (cleanup != nullptr && napi_remove_env_cleanup_hook(env, CleanupSelection, cleanup) == napi_ok) {
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->cleanup = nullptr;
        }
        delete cleanup;
    }
    CloseSelection(state);
    napi_handle_scope scope = nullptr;
    if (napi_open_handle_scope(env, &scope) != napi_ok) {
        if (callback != nullptr) { napi_delete_reference(env, callback); }
        HILOG_ERROR("Cannot open contact picker result scope");
        return;
    }
    napi_value value = nullptr;
    napi_value error = nullptr;
    napi_get_undefined(env, &error);
    napi_get_undefined(env, &value);
    // The internal result remains explicit even for failures. The public JS
    // wrapper maps it to a BusinessError, never to a partial successful array.
    napi_value code = nullptr;
    napi_value total = nullptr;
    napi_value json = nullptr;
    napi_create_object(env, &value);
    napi_create_int32(env, result.code, &code);
    napi_create_int32(env, result.total, &total);
    napi_create_string_utf8(env, result.data.c_str(), result.data.size(), &json);
    napi_set_named_property(env, value, "resultCode", code);
    napi_set_named_property(env, value, "total", total);
    napi_set_named_property(env, value, "pickerData", json);
    if (deferred != nullptr) {
        napi_resolve_deferred(env, deferred, value);
    } else if (callback != nullptr) {
        napi_value function = nullptr;
        napi_value receiver = nullptr;
        napi_value ignored = nullptr;
        napi_get_reference_value(env, callback, &function);
        napi_get_undefined(env, &receiver);
        napi_value args[] = { error, value };
        napi_call_function(env, receiver, function, 2, args, &ignored);
        napi_delete_reference(env, callback);
    }
    napi_close_handle_scope(env, scope);
}

void NotifySelection(const Selection &state)
{
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->alive && !state->settled && state->delivery != nullptr) {
        // uv_async_send is thread safe; all NAPI work happens on this env's loop.
        // A pending notification is coalesced, and the terminal result is immutable.
        if (uv_async_send(state->delivery) != 0) { HILOG_ERROR("Contact picker result delivery unavailable"); }
    }
}
} // namespace

napi_value ContactsNapiUtils::StartContactSelection(napi_env env,
    const std::shared_ptr<AbilityRuntime::AbilityContext> &ability,
    const std::shared_ptr<AbilityRuntime::UIExtensionContext> &extension,
    const AAFwk::Want &want, napi_ref callback)
{
    auto state = std::make_shared<SelectionRequest>();
    state->env = env;
    state->ability = ability;
    state->extension = extension;
    state->callback = callback;
    napi_value promise = nullptr;
    napi_get_undefined(env, &promise);
    if (callback == nullptr && napi_create_promise(env, &state->deferred, &promise) != napi_ok) {
        napi_throw_error(env, nullptr, "Cannot create contact picker promise");
        return nullptr;
    }
    state->cleanup = new std::weak_ptr<SelectionRequest>(state);
    if (napi_add_env_cleanup_hook(env, CleanupSelection, state->cleanup) != napi_ok) {
        delete state->cleanup;
        state->cleanup = nullptr;
        state->protocol.Fail();
        CompleteSelection(state);
        return promise;
    }
    uv_loop_t *loop = nullptr;
    if (napi_get_uv_event_loop(env, &loop) != napi_ok || loop == nullptr) {
        state->protocol.Fail(); CompleteSelection(state); return promise;
    }
    auto delivery = new uv_async_t {};
    if (uv_async_init(loop, delivery, [](uv_async_t *handle) {
        auto state = *static_cast<Selection *>(handle->data);
        CompleteSelection(state);
    }) != 0) {
        delete delivery;
        state->protocol.Fail(); CompleteSelection(state); return promise;
    }
    delivery->data = new Selection(state);
    state->delivery = delivery;
    uv_unref(reinterpret_cast<uv_handle_t *>(delivery));
    auto deadline = new uv_timer_t {};
    if (uv_timer_init(loop, deadline) != 0) {
        delete deadline;
        state->protocol.Fail(); CompleteSelection(state); return promise;
    }
    deadline->data = new Selection(state);
    state->deadline = deadline;
    uv_unref(reinterpret_cast<uv_handle_t *>(deadline));
    if (uv_timer_start(deadline, [](uv_timer_t *handle) {
        auto state = *static_cast<Selection *>(handle->data);
        state->protocol.Fail();
        CompleteSelection(state);
    }, SELECTION_TIMEOUT_MS, 0) != 0) {
        state->protocol.Fail(); CompleteSelection(state); return promise;
    }
    state->content = ability != nullptr ? ability->GetUIContent() :
        (extension != nullptr ? extension->GetUIContent() : nullptr);
    if (state->content == nullptr) {
        state->protocol.Fail(); CompleteSelection(state); return promise;
    }
    Ace::ModalUIExtensionCallbacks callbacks {
        .onRelease = [state](int32_t) { state->protocol.Fail(); NotifySelection(state); },
        .onResult = [state](int32_t code, const AAFwk::Want &) {
            state->protocol.Finish(code); NotifySelection(state);
        },
        .onReceive = [state](const AAFwk::WantParams &params) {
            const bool valid = AAFwk::IInteger::Query(params.GetParam("index")) != nullptr &&
                AAFwk::IInteger::Query(params.GetParam("total")) != nullptr &&
                AAFwk::IString::Query(params.GetParam("pickerData")) != nullptr;
            if (state->protocol.Receive(valid, params.GetIntParam("index", -1),
                params.GetIntParam("total", -1), valid ? params.GetStringParam("pickerData") : "")) {
                NotifySelection(state);
            }
        },
        .onError = [state](int32_t, const std::string &, const std::string &) {
            state->protocol.Fail(); NotifySelection(state);
        },
        .onRemoteReady = [](const std::shared_ptr<Ace::ModalUIExtensionProxy> &) {},
        .onDestroy = [state] {
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                state->content = nullptr;
                state->closed = true;
            }
            state->protocol.Fail(); NotifySelection(state);
        },
    };
    Ace::ModalUIExtensionConfig config;
    config.isProhibitBack = true;
    const bool dismiss = want.GetBoolParam("isAutoDismissOnNavigation", false);
    config.prohibitedRemoveByRouter = !dismiss;
    config.prohibitedRemoveByNavigation = !dismiss;
    const int32_t session = state->content->CreateModalUIExtension(want, callbacks, config);
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->session = session;
    }
    if (session <= 0) { state->protocol.Fail(); NotifySelection(state); }
    return promise;
}

ModalUICallback::ModalUICallback(Ace::UIContent* uiContent,
    std::unique_ptr<OHOS::ContactsApi::BaseContext>& baseContext)
{
    this->uiContent = uiContent;
    this->baseContext = baseContext.get();
    this->pickerCallBack_ = baseContext->pickerCallBack.get();
}

void ModalUICallback::SetSessionId(int32_t sessionId)
{
    this->sessionId_=sessionId;
}

void ModalUICallback::OnRelease(int32_t releaseCode)
{
    HILOG_INFO("OnRelease enter. release code is %{public}d", releaseCode);
    this->uiContent->CloseModalUIExtension(this->sessionId_);
    this->pickerCallBack_->ready = true;
}

void ModalUICallback::OnError(int32_t code, const std::string& name, const std::string& message)
{
    HILOG_ERROR("OnError enter. errorCode=%{public}d, name=%{public}s, message=%{public}s",
        code, name.c_str(), message.c_str());
    if (!this->pickerCallBack_->ready) {
        this->uiContent->CloseModalUIExtension(this->sessionId_);
        this->pickerCallBack_->ready = true;
    }
}

void ModalUICallback::OnResultForModal(int32_t resultCode, const OHOS::AAFwk::Want &result)
{
    HILOG_INFO("OnResultForModal enter. resultCode is %{public}d", resultCode);
    this->pickerCallBack_->resultCode = resultCode;
}

void ModalUICallback::OnReceive(const OHOS::AAFwk::WantParams &request)
{
    HILOG_INFO("OnReceive enter.");
    bool isFromSaveContactPicker = request.GetParam("isFromSaveContactPicker");
    if (isFromSaveContactPicker) {
        int32_t contactIndex = request.GetIntParam("contactIndex", -1);
        this->pickerCallBack_->dataIndex = contactIndex;
        this->uiContent->CloseModalUIExtension(this->sessionId_);
        this->pickerCallBack_->ready = true;
        HILOG_WARN("%{public}s isFromSaveContactPicker  contactIndex : %{public}d ",
            __func__, contactIndex);
    } else {
        int32_t tempIndex = request.GetIntParam("index", -1);
        this->pickerCallBack_->dataIndex =
            (this->pickerCallBack_->dataIndex < tempIndex ? tempIndex : this->pickerCallBack_->dataIndex);
        this->pickerCallBack_->total = request.GetIntParam("total", -1);
        if (tempIndex <= (PICKER_MAX - 1) && tempIndex >= 0) {
            this->pickerCallBack_->pickerData[tempIndex] = request.GetStringParam("pickerData");
        }
        HILOG_WARN("%{public}s total : %{public}d index : %{public}d "
            "tempIndex : %{public}d pickerData size %{public}zu",
            __func__, this->pickerCallBack_->total, this->pickerCallBack_->dataIndex, tempIndex,
            sizeof(this->pickerCallBack_->pickerData));
    }
}

void ModalUICallback::OnDestroy()
{
    HILOG_INFO("OnDestroy enter.");
}

SyncModalCallback::SyncModalCallback(Ace::UIContent* uiContent, SyncDialogCallback* syncCallback)
{
    this->uiContent_ = uiContent;
    this->syncCallback_ = syncCallback;
}

void SyncModalCallback::SetSessionId(int32_t sessionId)
{
    this->sessionId_ = sessionId;
}

void SyncModalCallback::OnRelease(int32_t releaseCode)
{
    if (this->uiContent_ != nullptr) {
        this->uiContent_->CloseModalUIExtension(this->sessionId_);
    }
    if (this->syncCallback_ != nullptr) {
        std::lock_guard<std::mutex> lock(this->syncCallback_->mutex);
        if (this->syncCallback_->confirmResult == CONFIRM_RESULT_NO) {
            this->syncCallback_->confirmResult = CONFIRM_RESULT_CANCELLED;
        }
        this->syncCallback_->ready = true;
        this->syncCallback_->cv.notify_one();
    }
}

void SyncModalCallback::OnError(int32_t code, const std::string &name, const std::string &message)
{
    if (this->syncCallback_ != nullptr && !this->syncCallback_->ready) {
        if (this->uiContent_ != nullptr) {
            this->uiContent_->CloseModalUIExtension(this->sessionId_);
        }
        {
            std::lock_guard<std::mutex> lock(this->syncCallback_->mutex);
            this->syncCallback_->ready = true;
            this->syncCallback_->confirmResult = CONFIRM_RESULT_CANCELLED;
            this->syncCallback_->cv.notify_one();
        }
    }
}

void SyncModalCallback::OnResultForModal(int32_t resultCode, const OHOS::AAFwk::Want &result)
{
    if (this->uiContent_ != nullptr) {
        this->uiContent_->CloseModalUIExtension(this->sessionId_);
    }
}

void SyncModalCallback::OnReceive(const OHOS::AAFwk::WantParams &request)
{
    if (this->syncCallback_ != nullptr) {
        int32_t confirmResult = request.GetIntParam("confirmResult", CONFIRM_RESULT_NO);
        {
            std::lock_guard<std::mutex> lock(this->syncCallback_->mutex);
            this->syncCallback_->ready = true;
            this->syncCallback_->confirmResult = confirmResult;
            this->syncCallback_->cv.notify_one();
        }
    }
}

void SyncModalCallback::OnDestory()
{
}

} // namespace ContactsApi
} // namespace OHOS
