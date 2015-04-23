#include "jvmti.h"

#include "jni.h"

#include <string.h>

#include <unistd.h>

#include <stdlib.h>

typedef struct methodTiming MethodTiming;

struct methodTiming {

	jmethodID method;

	char* methodName;

	jlong starttime;

	jlong endtime;

	MethodTiming *next;

	MethodTiming *caller;

};

MethodTiming* head = NULL;
jthread mainThread;

MethodTiming* insert(jmethodID method, char* methodName, jlong starttime) {

	MethodTiming* temp = (MethodTiming *) malloc(sizeof(MethodTiming));

	temp->method = method;

	temp->starttime = starttime;

	temp->endtime = 0;

	temp->methodName = methodName;

	temp->next = head;

	temp->caller = NULL;

	head = temp;
	return temp;

}

MethodTiming* search(jmethodID method) {

	MethodTiming* temp = head;

	while (temp != NULL) {

		if (temp->method == method)

			return temp;

		temp = temp->next;

	}

	return NULL;

}

void JNICALL OnMethodEntry(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread, jmethodID method) {

	jvmtiFrameInfo frames[2];

	jvmtiError error;

	jlong time;

	jint count;

	char *methodName;

	if ((error = (*jvmti)->GetStackTrace(jvmti, thread, 0, 2, frames, &count)) != JVMTI_ERROR_NONE || count == 0) {

		printf("Error stack trace\n");
		return;
	}

	if((error = (*jvmti)->GetCurrentThreadCpuTime(jvmti, &time)) != JVMTI_ERROR_NONE) {

		printf("Error method entry\n");

		return;

	}

	if ((error = (*jvmti)->GetMethodName(jvmti, method, &methodName, NULL, NULL)) != JVMTI_ERROR_NONE) {

		printf("Error method name\n");

		return;

	}

	MethodTiming* timing = search(frames[0].method);
	timing = timing == NULL ? insert(frames[0].method, methodName, time) : timing;
	if (count > 1) {
		timing->caller = search(frames[1].method);
	}
}

void JNICALL OnMethodExit(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread, jmethodID method, jboolean was_popped_by_exception, jvalue return_value) {

	jvmtiError error;
	jlong time;

	if((error = (*jvmti)->GetCurrentThreadCpuTime(jvmti, &time)) != JVMTI_ERROR_NONE) {

		printf("Error method exit");

		return;

	}

	search(method)->endtime = time;;

}

void JNICALL OnVMInit(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread) {
	mainThread = thread;

	//printf("onVMInit\n");

}

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* _vm, char* options, void* reserved) {

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

	if (potentialCapabilities.can_get_thread_cpu_time) {

		requestedCapabilities.can_get_thread_cpu_time = 1;

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

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *_vm) {

	MethodTiming* temp = head;

	while (temp != NULL) {

		printf("%s %lu %lu %lu", temp->methodName, (long)temp->starttime, (long) temp->endtime, (long)(temp->endtime - temp->starttime));
		if (temp->caller != NULL) {
			printf("\t\t%s\n", temp->caller->methodName);
		}

		temp = temp->next;

	}

}

