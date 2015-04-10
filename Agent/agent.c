#include "jvmti.h"
#include "jni.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>



struct {
	char* methodName, 
	methodTiming* caller,
	long selftime,
	long wallclocktime
}methodTiming;

char* clazz;
char* method;
char* param;
char* ret;
jmethodID method_id = NULL;
struct methodTiming* methodArray;
int methodCount = 0;

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

	
	if (options != NULL && options[0] != '\0') {
		char* nextArgument = options;
		const char delimiters[] = ":=";
		char *key, *value;
		int i = 0;

		do {
			if(i==0) key = strtok(nextArgument, delimiters);
			else key = strtok(NULL, delimiters);

			if(key != NULL){
				value = strtok(NULL, delimiters);
				if(strstr(key, "clazz")!=NULL) clazz = value;
				else if(strstr(key, "method")!=NULL) method = value;
				else if(strstr(key, "param")!=NULL) param = value;
				else if(strstr(key, "ret")!=NULL) ret = value;
			}

			i++;
		} while(key != NULL); 
	}
	
	
	
	
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
	callbacks.MethodEntry = OnMethodEntry;
	callbacks.MethodExit = OnMethodExit;

	(*jvmti)->SetEventCallbacks(jvmti, &callbacks, sizeof(callbacks));
	(*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, NULL);
	(*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_METHOD_ENTRY, NULL);
	(*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_METHOD_EXIT, NULL);
	return 0;
}

JNIEXPORT void JNICALL Agent_UnLoad() {
	// print 10 hottest methods
}

void JNICALL OnMethodEntry(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread, jmethodID method)
{
	if(method == method_id)
	{
		//Take time
		//add to struct
	}
	
}

void JNICALL OnMethodExit(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread, jmethodID method, jboolean was_popped_by_exception, jvalue return_value)
{
	if(method == method_id)
	{
		//Take time
		//take time in struct
	}
}
