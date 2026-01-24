#ifndef MI_E2EE_PLATFORM_ANDROID_JNI_H
#define MI_E2EE_PLATFORM_ANDROID_JNI_H

#ifdef __ANDROID__
#include <jni.h>

namespace mi::platform::android {

void SetJavaVm(JavaVM* vm);
void RegisterSecureStore(JNIEnv* env);

}  // namespace mi::platform::android
#endif

#endif  // MI_E2EE_PLATFORM_ANDROID_JNI_H
