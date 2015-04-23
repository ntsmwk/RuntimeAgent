#include "jvmti.h"
#include "jni.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

typedef struct methodTiming MethodTiming;
typedef struct treeNode TreeNode;
typedef struct nodeList NodeList;
#define MAX_FRAMES 10
#define INITIAL_CHILDREN_COUNT 2
#define CHILDREN_COUNT_MULTIPLIER 2

struct methodTiming {
	jmethodID method;
	char* methodName;
	int count;
};

struct treeNode {
	MethodTiming* method;
	TreeNode* children;
	int childrenCount;
};

TreeNode* createTreeNode(MethodTiming* methodTiming) {
	TreeNode* treeNode = (TreeNode*) malloc(sizeof(TreeNode));
	treeNode->method = methodTiming;
	treeNode->children = (TreeNode*) malloc(INITIAL_CHILDREN_COUNT * sizeof(TreeNode));
	treeNode->childrenCount = INITIAL_CHILDREN_COUNT;
	return treeNode;
}

MethodTiming* createMethodTiming(jmethodID method, char* methodName) {
	MethodTiming* timing = (MethodTiming*) malloc(sizeof(MethodTiming));
	timing->method = method;
	timing->methodName = methodName;
	return timing;
}

TreeNode* searchChildren(TreeNode* parent, jmethodID methodID){
	TreeNode* temp = parent->children;
	int counter = 0;
	while(&temp[counter] != NULL && counter < parent->childrenCount){
		if(temp[counter].method->method == methodID){
				return &temp[counter];
		}
		counter++;
	}
	return NULL;
}	

void copyAndExtendChildrenList(TreeNode* children, int childrenCount){
	TreeNode* temp = (TreeNode*) malloc(childrenCount*CHILDREN_COUNT_MULTIPLIER* sizeof(TreeNode));
	int count = 0;
	while(count < childrenCount){
		temp[count] = children[count];
	}
	free(children);
	children = temp;
}

void insertChildren(TreeNode* parent, TreeNode* insertChildren){
	TreeNode* list = parent->children;
	int count = 0;
	while(&list[count] != NULL){
		count++;
		//List is full
		if(count == parent->childrenCount){
			copyAndExtendChildrenList(parent->children, parent->childrenCount);
			memcpy(&parent->children[count], insertChildren, sizeof(TreeNode));
			free(insertChildren);
			return;
		}
	}
	memcpy(&list[count], insertChildren, sizeof(TreeNode));
	free(insertChildren);
	return;
}


void freeAll(TreeNode* top){
	TreeNode* temp = top->children;
	int count = 0;
	while(count < top->childrenCount && &temp[count] != NULL){
		freeAll(&temp[count]);
		count++;
	}
	free(top);
}

void JNICALL run(jvmtiEnv* jvmti, JNIEnv* jni, void* args) {
	jvmtiFrameInfo *frames;
	jvmtiStackInfo *stack_info;
	jint thread_count;
	char *methodName;
	jvmtiError error;
	jlong time;
	int ti;
	TreeNode* top = (TreeNode *) malloc(sizeof(TreeNode));

	if ((error = (*jvmti)->GetAllStackTraces(jvmti, MAX_FRAMES, &stack_info, &thread_count)) != JVMTI_ERROR_NONE) {
		printf("Error stack trace\n");
		return;
	}

	for (ti = 0; ti < thread_count; ++ti) {
		jvmtiStackInfo *infop = &stack_info[ti];
		frames = infop->frame_buffer;
		int fi;
		TreeNode* current = top;
		
		for (fi = 0; fi < infop->frame_count; fi++) {
			if ((error = (*jvmti)->GetMethodName(jvmti, frames[fi].method, &methodName, NULL, NULL)) != JVMTI_ERROR_NONE) {
				printf("Error method name\n");
				return;
			}
			
			//Build tree
			TreeNode* children = searchChildren(current, frames[fi].method);
			if(children == NULL){
				MethodTiming* childMethod = createMethodTiming(frames[fi].method, methodName);
				TreeNode* childNode = createTreeNode(childMethod);
				insertChildren(current, childNode);
			}
			else{
				children->method->count++;
				current = children;
			}			
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
	
	freeAll(top);
	
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

