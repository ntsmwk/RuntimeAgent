#include "jvmti.h"

#include "jni.h"

#include <string.h>

#include <unistd.h>

#include <stdlib.h>

typedef struct methodTiming MethodTiming;

typedef struct treeNode TreeNode;

#define MAX_FRAMES 10

struct methodTiming {

	jmethodID method;

	char* methodName;

	int count;

};

struct treeNode {

	MethodTiming* methodTiming;

	TreeNode* chidren;

};

TreeNode * createTreeNode(MethodTiming* methodTiming) {
	TreeNode* treeNode = (TreeNode*) malloc(sizeof(TreeNode));
	treeNode->methodTiming = methodTiming;
	return treeNode;
}

MethodTiming* createMethodTiming(jmethodID method, char* methodName) {

	MethodTiming* timing = (MethodTiming*) malloc(sizeof(MethodTiming));

	timing->method = method;

	timing->methodName = methodName;

	return timing;

}

void JNICALL run(jvmtiEnv* jvmti, JNIEnv* jni, void* args) {

	jvmtiFrameInfo frames[2];

	jvmtiStackInfo *stack_info;

	jint thread_count;

	char *methodName;
	jvmtiError error;

	jlong time;

	TreeNode* parent = (TreeNode *) malloc(sizeof(TreeNode));

	if ((error = (*jvmti)->GetAllStackTraces(jvmti, MAX_FRAMES, &stack_info, &thread_count)) != JVMTI_ERROR_NONE) {

		printf("Error stack trace\n");

		return;

	}

	int ti;

	for (ti = 0; ti < thread_count; ++ti) {

		jvmtiStackInfo *infop = &stack_info[ti];

		jvmtiFrameInfo *frames = infop->frame_buffer;

		int fi;
		TreeNode * current = parent;

		for (fi = 0; fi < infop->frame_count; fi++) {

			TreeNode* children = parent->children;
			while(children != NULL) {
				if (children->methodTiming->method == frames[fi].method) {
					break;
				}
				children = children->children;
			}

			if ((error = (*jvmti)->GetMethodName(jvmti, frames[fi].method, &methodName, NULL, NULL)) != JVMTI_ERROR_NONE) {

				printf("Error method name\n");

				return;

			}

			//MethodTiming* timing = createMethodTiming(frames[fi].method, methodName);

			//if (current == NULL){
			//	current = createTreeNode(timing);
			//}

			//myFramePrinter(frames[fi].method, frames[fi].location);

		}
	}

	/*if((error = (*jvmti)->GetCurrentThreadCpuTime(jvmti, &time)) != JVMTI_ERROR_NONE){

	 printf("Error current thread cpu time %s\n", methodName);

	 return;

	 }*/

	if ((error = (*jvmti)->Deallocate(jvmti, stack_info)) != JVMTI_ERROR_NONE) {

		printf("Error deallocate stack info\n");

		return;

	}

	printf("Hello World");

}

void JNICALL OnVMInit(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread) {

	jclass thread_class = (*jni)->FindClass(jni, "java/lang/Thread");

	jmethodID ctor_id = (*jni)->GetMethodID(jni, thread_class, "<init>", "()V");

	jthread newthread = (jthread) (*jni)->NewObject(jni, thread_class, ctor_id);

	(*jvmti)->RunAgentThread(jvmti, newthread, run, NULL, JVMTI_THREAD_MAX_PRIORITY);

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

		return -1;

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

	(*jvmti)->SetEventCallbacks(jvmti, &callbacks, sizeof(callbacks));

	(*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_INIT,
			NULL);

	return 0;

}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *_vm) {

	/*MethodTiming* temp = head;

	 while (temp != NULL) {

	 printf("%s %lu %lu %lu\n", temp->methodName, (long)temp->starttime, (long) temp->endtime, (long)(temp->endtime - temp->starttime));

	 temp = temp->next;

	 }*/

}

