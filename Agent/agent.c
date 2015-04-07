#include "jvmti.h"
#include "jni.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

void JNICALL OnVMInit(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread) {
	jvmtiStackInfo *stack_info;
	jint thread_count;
	jvmtiError err;

	if ((err = (*jvmti)->GetAllStackTraces(jvmti, MAX_FRAMES, &stack_info, &thread_count)) != JVMTI_ERROR_NONE) {
		return;
	}

	int ti;
	for (ti = 0; ti < thread_count; ++ti) {
		jvmtiStackInfo *infop = &stack_info[ti];
		jthread thread = infop->thread;
		jint state = infop->state;
		jvmtiFrameInfo *frames = infop->frame_buffer;

		//myThreadAndStatePrinter(thread, state);
		int fi;
		for (fi = 0; fi < infop->frame_count; fi++) {
			printf("%s", frames[fi].method, frames[fi].location);
		}
	}
	/* this one Deallocate call frees all data allocated by GetAllStackTraces */
	err = (*jvmti)->Deallocate(jvmti, stack_info);
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

	// enable method entry and exit capabilities
	if ((error = (*jvmti)->AddCapabilities(jvmti, &requestedCapabilities))
			!= JVMTI_ERROR_NONE)
		return 0;

	jvmtiEventCallbacks callbacks;
	memset(&callbacks, 0, sizeof(callbacks));
	callbacks.VMInit = OnVMInit;

	(*jvmti)->SetEventCallbacks(jvmti, &callbacks, sizeof(callbacks));
	(*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, NULL);
	return 0;
}

JNIEXPORT void JNICALL Agent_UnLoad() {
	// print 10 hottest methods
}
