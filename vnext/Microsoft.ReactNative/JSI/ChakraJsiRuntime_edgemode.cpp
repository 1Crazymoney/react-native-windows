// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <JSI/ByteArrayBuffer.h>
#include <JSI/ChakraRuntime.h>
#include <cxxreact/MessageQueueThread.h>
#include "Unicode.h"

#if !defined(CHAKRACORE)
#include <jsrt.h>

namespace Microsoft::JSI {

void ChakraRuntime::setupNativePromiseContinuation() noexcept {
  JsSetPromiseContinuationCallback(PromiseContinuationCallback, this);
}

// ES6 Promise callback
void CALLBACK ChakraRuntime::PromiseContinuationCallback(JsValueRef funcRef, void *callbackState) noexcept {
  ChakraRuntime *runtime = static_cast<ChakraRuntime *>(callbackState);
  runtime->PromiseContinuation(funcRef);
}

void ChakraRuntime::PromiseContinuation(JsValueRef funcRef) noexcept {
  if (runtimeArgs().jsQueue) {
    JsAddRef(funcRef, nullptr);
    runtimeArgs().jsQueue->runOnQueue([this, funcRef]() {
      JsValueRef undefinedValue;
      JsGetUndefinedValue(&undefinedValue);
      ChakraVerifyJsErrorElseThrow(JsCallFunction(funcRef, &undefinedValue, 1, nullptr));
      JsRelease(funcRef, nullptr);
    });
  }
}

void ChakraRuntime::startDebuggingIfNeeded() {
  if (runtimeArgs().enableDebugging)
    JsStartDebugging();
}

void ChakraRuntime::stopDebuggingIfNeeded() {
  // NOP AFAIK
}

void ChakraRuntime::initRuntimeVersion() noexcept {
  // NOP
}

std::unique_ptr<const facebook::jsi::Buffer> ChakraRuntime::generatePreparedScript(
    const std::string & /*sourceURL*/,
    const facebook::jsi::Buffer &sourceBuffer) noexcept {
  const std::wstring scriptUTF16 =
      Microsoft::Common::Unicode::Utf8ToUtf16(reinterpret_cast<const char *>(sourceBuffer.data()), sourceBuffer.size());

  unsigned long bytecodeSize = 0;
  if (JsSerializeScript(scriptUTF16.c_str(), nullptr, &bytecodeSize) == JsNoError) {
    std::unique_ptr<ByteArrayBuffer> bytecodeBuffer(std::make_unique<ByteArrayBuffer>(bytecodeSize));
    if (JsSerializeScript(scriptUTF16.c_str(), bytecodeBuffer->data(), &bytecodeSize) == JsNoError) {
      return bytecodeBuffer;
    }
  }

  return nullptr;
}

facebook::jsi::Value ChakraRuntime::evaluateJavaScriptSimple(
    const facebook::jsi::Buffer &buffer,
    const std::string &sourceURL) {
  const std::wstring script16 =
      Microsoft::Common::Unicode::Utf8ToUtf16(reinterpret_cast<const char *>(buffer.data()), buffer.size());
  if (script16.empty())
    throw facebook::jsi::JSINativeException("Script can't be empty.");

  const std::wstring url16 = Microsoft::Common::Unicode::Utf8ToUtf16(sourceURL);

  JsValueRef result;
  ChakraVerifyJsErrorElseThrow(
      JsRunScript(script16.c_str(), JS_SOURCE_CONTEXT_NONE /*sourceContext*/, url16.c_str(), &result));

  return ToJsiValue(result);
}

bool ChakraRuntime::evaluateSerializedScript(
    const facebook::jsi::Buffer &scriptBuffer,
    const facebook::jsi::Buffer &serializedScriptBuffer,
    const std::string &sourceURL,
    JsValueRef *result) {
  std::wstring script16 =
      Microsoft::Common::Unicode::Utf8ToUtf16(reinterpret_cast<const char *>(scriptBuffer.data()), scriptBuffer.size());
  std::wstring url16 = Microsoft::Common::Unicode::Utf8ToUtf16(sourceURL);

  // Note:: Bytecode caching on UWP is untested yet.
  JsErrorCode errorCode = JsRunSerializedScript(
      script16.c_str(), const_cast<uint8_t *>(serializedScriptBuffer.data()), 0, url16.c_str(), result);

  if (errorCode == JsNoError) {
    return true;
  } else if (errorCode == JsErrorBadSerializedScript) {
    return false;
  } else {
    ChakraVerifyJsErrorElseThrow(errorCode);
    return true;
  }
}

} // namespace Microsoft::JSI

#endif
