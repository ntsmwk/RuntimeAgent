#include "jvmti.h"
#include "jni.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
/*
 #typedef struct methodTiming MethodTiming

 struct {
 char* methodName,
 MethodTiming* caller,
 long selftime,
 long wallclocktime
 } methodTiming;

 MethodTiming* methods;
 */

void JNICALL OnMethodEntry(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread, jmethodID method) {
	jlong time = NULL;
	jvmtiError error;
	if((error = (*jvmti)->GetCurrentThreadCpuTime(jvmti, jthread, &time)) != JVMTI_ERROR_NONE) {
		return;
	}

	printf("%lu\n", time);
}

void JNICALL OnMethodExit(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread, jmethodID method, jboolean was_popped_by_exception, jvalue return_value) {

	jlong time = NULL;
	jvmtiError error;
	if((error = (*jvmti)->GetCurrentThreadCpuTime(jvmti, jthread, &time)) != JVMTI_ERROR_NONE) {
		return;
	}
	printf("%lu\n", time);
}

void JNICALL OnVMInit(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread) {
	printf("Init");
}

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* _vm, char* options, void* reserved) {
	printf("Loading");
	jvmtiEnv* jvmti = NULL;
	(*_vm)->GetEnv(_vm, (void**) &jvmti, JVMTI_VERSION);

	jvmtiError error;
	jvmtiCapabilities requestedCapabilities, potentialCapabilities;
	memset(&requestedCapabilities, 0, sizeof(requestedCapabilities));

	// error checks
	if ((error = (*jvmti)->GetPotentialCapabilities(jvmti,
			&potentialCapabilities)) != JVMTI_ERROR_NONE) {
		return 0;
	}

	if (potentialCapabilities.can_generate_method_entry_events) {
		requestedCapabilities.can_generate_method_entry_events = 1;
	}
	if (potentialCapabilities.can_generate_method_exit_events) {
		requestedCapabilities.can_generate_method_exit_events = 1;
	}
	if (potentialCapabilities.can_access_local_variables) {
		requestedCapabilities.can_access_local_variables = 1;
	}
	if (potentialCapabilities.can_get_current_thread_cpu_time) {
		requestedCapabilities.can_get_current_thread_cpu_time = 1;
	}

	// enable method entry and exit capabilities
	if ((error = (*jvmti)->AddCapabilities(jvmti, &requestedCapabilities))
			!= JVMTI_ERROR_NONE) {
		return 0;
	}

	jvmtiEventCallbacks callbacks;
	memset(&callbacks, 0, sizeof(callbacks));
	callbacks.VMInit = OnVMInit;
	callbacks.MethodEntry = OnMethodEntry;
	callbacks.MethodExit = OnMethodExit;

	(*jvmti)->SetEventCallbacks(jvmti, &callbacks, sizeof(callbacks));
	(*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_INIT,
			NULL);
	(*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
			JVMTI_EVENT_METHOD_ENTRY, NULL);
	(*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
			JVMTI_EVENT_METHOD_EXIT, NULL);
	return 0;
}

JNIEXPORT void JNICALL Agent_UnLoad() {
	// print 10 hottest methods
}
