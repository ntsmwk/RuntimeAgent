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

void insert(jmethodID method, char* methodName, jlong starttime) {

	MethodTiming* temp = (MethodTiming *) malloc(sizeof(MethodTiming));

	temp->method = method;

	temp->starttime = starttime;

	temp->endtime = 0;

	temp->methodName = methodName;

	temp->next = head;

	temp->caller = NULL;

	head = temp;

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

	jvmtiError error;

	jvmtiFrameInfo frames[2];

	jlong time = NULL;

	jint count;

	char *methodName;

	if((error = (*jvmti)->GetCurrentThreadCpuTime(jvmti, &time)) != JVMTI_ERROR_NONE) {

		printf("Error method entry\n");

		return;

	}

	if ((error = (*jvmti)->GetMethodName(jvmti, method, &methodName, NULL, NULL)) != JVMTI_ERROR_NONE) {

		printf("Error method name\n");

		return;

	}

	if ((error = (*jvmti)->GetStackTrace(jvmti, thread, 1, 2, &frames, &count)) == JVMTI_ERROR_NONE && count > 1) {

		insert(method, methodName, time);

		if (count >= 2) {
			MethodTiming* node = search(frames[0].method);

			head->caller = node;

		}

	} else {
		insert(method, methodName, time);
	}

}

void JNICALL OnMethodExit(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread, jmethodID method, jboolean was_popped_by_exception, jvalue return_value) {

	jlong time = NULL;

	jvmtiError error;

	int i = 0;

	if((error = (*jvmti)->GetCurrentThreadCpuTime(jvmti, &time)) != JVMTI_ERROR_NONE) {

		printf("Error method exit");

		return;

	}

	MethodTiming* methodTiming = search(method);
	if (methodTiming != NULL) {
		methodTiming->endtime = time;
	}

}

void JNICALL OnVMInit(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread) {

	printf("onVMInit\n");

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

		printf("%s %lu %lu %lu", temp->methodName, (long)temp->starttime, (long) temp->endtime, (long)temp->endtime - temp->starttime);

		MethodTiming * caller = head->caller;

		while(caller != NULL) {

			printf("\t %s", caller->methodName);

			caller = caller->caller;

		}

		printf("\n");

		temp = temp->next;

	}

}

