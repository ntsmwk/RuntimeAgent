#include "jvmti.h"
#include "jni.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

typedef struct methodTiming MethodTiming;
typedef struct treeNode TreeNode;
typedef struct nodeList NodeList;

#define CHILDREN_COUNT_MULTIPLIER 2
#define INITIAL_CHILDREN_COUNT 2
#define MAX_FRAMES 10
#define TRUE 1
#define FALSE 0

struct methodTiming {
	jmethodID method;
	char* methodName;
	int count;
};

struct treeNode {
	MethodTiming* method;
	TreeNode* children;
	int childrenCount;
	int isSet;
};

struct nodeList {
	jmethodID method;
	char * methodName;
	int selfTime;
	int wallClockTime;
	NodeList * next;
};

TreeNode* top;
jlong starttime, endtime;
int stackCount;
time_t starttime_c, endtime_c;

NodeList* list;

TreeNode* createTreeNode(MethodTiming* methodTiming) {
	TreeNode* treeNode = (TreeNode*) malloc(sizeof(TreeNode));
	treeNode->method = methodTiming;
	treeNode->children = (TreeNode*) malloc(INITIAL_CHILDREN_COUNT * sizeof(TreeNode));
	treeNode->childrenCount = INITIAL_CHILDREN_COUNT;
	treeNode->children[0].isSet = FALSE;
	treeNode->children[1].isSet = FALSE;
	return treeNode;
}

MethodTiming* createMethodTiming(jmethodID method, char* methodName) {
	MethodTiming* timing = (MethodTiming*) malloc(sizeof(MethodTiming));
	timing->method = method;
	timing->methodName = methodName;
	timing->count = 1;
	return timing;
}

TreeNode* searchChildren(TreeNode* parent, jmethodID methodID){
	TreeNode* temp = parent->children;
	int counter = 0;
	
	while(&(temp[counter]) != NULL && temp[counter].isSet && counter < parent->childrenCount){
		if(temp[counter].method->method == methodID){
			return &temp[counter];
		}
		counter++;
	}
	return NULL;
}	

void copyAndExtendChildrenList(TreeNode* parent){
	TreeNode* temp = (TreeNode*) malloc(parent->childrenCount*CHILDREN_COUNT_MULTIPLIER* sizeof(TreeNode));	
	memcpy(temp,parent->children, parent->childrenCount* sizeof(TreeNode)); 
		
	//TODO find out why not possible....
	//free(parent->children);
	parent->children = temp;
	parent->childrenCount = parent->childrenCount*CHILDREN_COUNT_MULTIPLIER;
}

TreeNode* insertChildren(TreeNode* parent, TreeNode* insertChildren){
	TreeNode* list = parent->children;
	int count = 0;
	while(list[count].isSet){
		//List is full
		if(count == parent->childrenCount){
			copyAndExtendChildrenList(parent);
			break;	
		}
		count++;
	}
	memcpy(&(list[count]), insertChildren, sizeof(TreeNode));
	//free(insertChildren);
	list[count].isSet = TRUE;
	return &(list[count]);
}

NodeList * insertNodeEntry(jmethodID method, char * methodName){
	NodeList * node = (NodeList *) malloc(sizeof(NodeList));
	node->method = method;
	node->methodName = methodName;
	node->selfTime = 0;
	node->wallClockTime = 0;
	node->next = list;
	list = node;
	return list;
}

void freeAll(TreeNode* top){
	TreeNode* temp = top->children;
	int count = 0;
	while(count < top->childrenCount && &temp[count] != NULL){
		freeAll(&(temp[count]));
		count++;
	}
	free(top);
}

void printTree(TreeNode* top, int level){	
	if(top->method != NULL && top->method->methodName != NULL){
		printf("%d %s %d\n", level, top->method->methodName, top->method->count);
	} else{
		printf("%d TOP\n", level);	
	}
	
	TreeNode* children = top->children;
	int count = 0;
	while(count < top->childrenCount && &(children[count]) != NULL && children[count].isSet){
		printTree(&(children[count]), level+1);
		count++;
	}
}

NodeList * searchNodeEntry(jmethodID method){
	NodeList* current = list;
	while(current != NULL){
		if (current->method == method){
			return current;		
		}
		current = current->next;	
	}
	return NULL;
}

void buildMethodList(TreeNode * node, int level){
	int count = 0;
	TreeNode * children = node->children;
	while(count < node->childrenCount && &(children[count]) != NULL && children[count].isSet){
		jmethodID methodId = children[count].method->method;
		int occurrences = children[count].method->count;
		NodeList * entry = searchNodeEntry(methodId);
		entry = entry == NULL ? insertNodeEntry(methodId, children[count].method->methodName) : entry; 
		printf("Level; %d, %s\n", level, entry->methodName);
		if (level == 0){
			entry->selfTime += occurrences;
		} else {
			entry->wallClockTime += occurrences;
		}
		buildMethodList(&(children[count]), level + 1);
		count++;
	}
}

void JNICALL run(jvmtiEnv* jvmti, JNIEnv* jni, void* args) {
	jvmtiFrameInfo *frames;
	jvmtiStackInfo *stack_info;
	jint thread_count;
	char *methodName;
	jvmtiError error;
	int ti;

	stackCount = 0;
	top = (TreeNode *) malloc(sizeof(TreeNode));
	top->children = (TreeNode*) malloc(INITIAL_CHILDREN_COUNT * sizeof(TreeNode));
	top->childrenCount = INITIAL_CHILDREN_COUNT;


	while(TRUE){
		if ((error = (*jvmti)->GetAllStackTraces(jvmti, MAX_FRAMES, &stack_info, &thread_count)) != JVMTI_ERROR_NONE) {
			break;
		}

		for (ti = 0; ti < thread_count; ++ti) {
			jvmtiStackInfo *infop = &stack_info[ti];
			frames = infop->frame_buffer;
			int fi;
			TreeNode* current = top;
			stackCount++;

			for (fi = infop->frame_count - 1; fi >= 0; fi--) {
				if((error = (*jvmti)->GetCurrentThreadCpuTime(jvmti, &endtime)) != JVMTI_ERROR_NONE){
					printf("time error %d", error);	
					break;		
				}
				if ((error = (*jvmti)->GetMethodName(jvmti, frames[fi].method, &methodName, NULL, NULL)) != JVMTI_ERROR_NONE) {
					break;
				}

				TreeNode* children = searchChildren(current, frames[fi].method);				
				if(children == NULL){
					MethodTiming* childMethod = createMethodTiming(frames[fi].method, methodName);
					TreeNode* childNode = createTreeNode(childMethod);
					current = insertChildren(current, childNode);
				}
				else{
					children->method->count++;
					current = children;
				}
			}
		}
				
		//TODO find out why not possible...
		//freeAll(top);
	}
	
	time(&endtime_c);
	if ((error = (*jvmti)->Deallocate(jvmti, (unsigned char *)stack_info)) != JVMTI_ERROR_NONE) {
		printf("Error deallocate stack info\n");
	}
}

void JNICALL OnVMInit(jvmtiEnv *jvmti, JNIEnv* jni, jthread thread) {
	jclass thread_class = (*jni)->FindClass(jni, "java/lang/Thread");
	jmethodID ctor_id = (*jni)->GetMethodID(jni, thread_class, "<init>", "()V");
	jthread newthread = (jthread) (*jni)->NewObject(jni, thread_class, ctor_id);
	(*jvmti)->RunAgentThread(jvmti, newthread, run, NULL, JVMTI_THREAD_MAX_PRIORITY);
}

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* _vm, char* options, void* reserved) {
	jvmtiEnv* jvmti = NULL;
	jvmtiError error;
	(*_vm)->GetEnv(_vm, (void**) &jvmti, JVMTI_VERSION);

	jvmtiCapabilities requestedCapabilities, potentialCapabilities;
	memset(&requestedCapabilities, 0, sizeof(requestedCapabilities));
	if ((error = (*jvmti)->GetPotentialCapabilities(jvmti, &potentialCapabilities)) != JVMTI_ERROR_NONE) {
		return -1;
	}

	if (potentialCapabilities.can_access_local_variables) {
		requestedCapabilities.can_access_local_variables = 1;
	}
	if (potentialCapabilities.can_get_current_thread_cpu_time) {
		requestedCapabilities.can_get_current_thread_cpu_time = 1;
	}

	if ((error = (*jvmti)->AddCapabilities(jvmti, &requestedCapabilities)) != JVMTI_ERROR_NONE) {
		return -1;
	}

	jvmtiEventCallbacks callbacks;
	memset(&callbacks, 0, sizeof(callbacks));
	callbacks.VMInit = OnVMInit;
	(*jvmti)->SetEventCallbacks(jvmti, &callbacks, sizeof(callbacks));
	(*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, NULL);

	if((error = (*jvmti)->GetCurrentThreadCpuTime(jvmti, &starttime)) != JVMTI_ERROR_NONE){
		printf("time error %d", error);
		return 0;		
	}

	time(&starttime_c);
	return 0;
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *_vm) {	
	buildMethodList(top, 0);
	
	printTree(top,0);

	NodeList * node = list;
	printf("Top Methods\n");
	while(node != NULL){
		printf("%s %d %d\n", node->methodName, node->selfTime, node->wallClockTime);
		node = node->next;	
	}
	printf("\nRuntime: %lu \n StackCount: %d\n", (long) (endtime-starttime), stackCount);
	printf("\nRuntime: %lu \n", endtime_c-starttime_c);
	
}

