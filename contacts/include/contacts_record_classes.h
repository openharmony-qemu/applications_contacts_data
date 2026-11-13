/*
 * SPDX-License-Identifier: Apache-2.0
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

#ifndef CONTACTS_RECORD_CLASSES_H
#define CONTACTS_RECORD_CLASSES_H

#include "contacts_napi_utils.h"

namespace OHOS {
namespace ContactsApi {
inline napi_value CreatePublicHolderRecord(napi_env env, napi_callback_info info)
{
    napi_value result = ContactsNapiUtils::CreateClassConstructor(env, info);
    if (result == nullptr) {
        return nullptr;
    }
    napi_value bundleName = nullptr;
    if (napi_create_string_utf8(env, CONTACT_PACKAGE_NAME, NAPI_AUTO_LENGTH, &bundleName) != napi_ok ||
        napi_set_named_property(env, result, "bundleName", bundleName) != napi_ok) {
        return nullptr;
    }
    return result;
}

inline napi_status RegisterPublicContactRecords(napi_env env, napi_value exports)
{
    struct Record {
        const char *name;
        napi_callback constructor;
    };
    const Record records[] = {
        {"ContactAttributes", ContactsNapiUtils::CreateClassConstructor},
        {"Group", ContactsNapiUtils::CreateClassConstructor},
        {"Holder", CreatePublicHolderRecord},
        {"Name", ContactsNapiUtils::CreateClassConstructor},
        {"NickName", ContactsNapiUtils::CreateClassConstructor},
        {"Note", ContactsNapiUtils::CreateClassConstructor},
        {"Organization", ContactsNapiUtils::CreateClassConstructor},
        {"Portrait", ContactsNapiUtils::CreateClassConstructor},
        {"Website", ContactsNapiUtils::CreateClassConstructor},
    };
    for (const auto &record : records) {
        napi_value constructor = nullptr;
        napi_status status = napi_define_class(env, record.name, NAPI_AUTO_LENGTH, record.constructor,
            nullptr, 0, nullptr, &constructor);
        if (status != napi_ok) {
            return status;
        }
        status = napi_set_named_property(env, exports, record.name, constructor);
        if (status != napi_ok) {
            return status;
        }
    }
    return napi_ok;
}
} // namespace ContactsApi
} // namespace OHOS

#endif // CONTACTS_RECORD_CLASSES_H
