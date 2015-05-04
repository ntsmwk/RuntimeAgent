#include "jvmti.h"
#include "jni.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define CHILDREN_COUNT_MULTIPLIER 2
#define INITIAL_CHILDREN_COUNT 2
#define MAX_FRAMES 10
#define TRUE 1
#define FALSE 0

typedef struct methodTiming MethodTiming;
typedef struct treeNode TreeNode;

struct methodTiming {
	jmethodID method;
	char* methodName;
	double count;		//Wall-clock stack frames
	double selfCount;		//Self stack frames
};

struct treeNode {
	MethodTiming* method;
	TreeNode* children;
	TreeNode* parent;
	int childrenCount;
	int isSet;
	int isTraversed;
};

TreeNode* top;
jlong starttime, endtime;
int stackCount;
int sleepTime;

TreeNode* createTreeNode(MethodTiming* methodTiming, TreeNode* parent, jvmtiEnv* jvmti) {
	jvmtiError error;
	TreeNode* treeNode; 
	if((error = (*jvmti)->Allocate(jvmti, sizeof(TreeNode), (unsigned char **)(&treeNode)) != JVMTI_ERROR_NONE)){
		printf("Error allocating treeNode");
		return NULL;
	}
	treeNode->method = methodTiming;
	treeNode->parent = parent;
	if((error = (*jvmti)->Allocate(jvmti, INITIAL_CHILDREN_COUNT * sizeof(TreeNode), (unsigned char **)&(treeNode->children)) != JVMTI_ERROR_NONE)){
		printf("Error allocating treeNode children");
		return NULL;
	}

	treeNode->childrenCount = INITIAL_CHILDREN_COUNT;
	treeNode->isTraversed = FALSE;
	treeNode->children[0].isSet = FALSE;
	treeNode->children[1].isSet = FALSE;
	treeNode->children[0].isTraversed = FALSE;
	treeNode->children[1].isTraversed = FALSE;
	treeNode->children[0].parent = treeNode;
	treeNode->children[1].parent = treeNode;
	
	return treeNode;
}

MethodTiming* createMethodTiming(jmethodID method, char* methodName, jvmtiEnv* jvmti) {
	jvmtiError error;
	MethodTiming* timing;
	if((error = (*jvmti)->Allocate(jvmti, sizeof(MethodTiming), (unsigned char **) &timing) != JVMTI_ERROR_NONE)){
		printf("Error allocating method Timing");
		return NULL;
	}
	timing->method = method;
	timing->methodName = methodName;
	timing->count = 1;
	timing->selfCount = 1;
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

void copyAndExtendChildrenList(TreeNode* parent, jvmtiEnv* jvmti){
	jvmtiError error;
	TreeNode* temp;
	if((error = (*jvmti)->Allocate(jvmti, parent->childrenCount*CHILDREN_COUNT_MULTIPLIER*sizeof(TreeNode), (unsigned char **) &temp) != JVMTI_ERROR_NONE)){
		printf("Error allocating Extended Tree Node Children List");
		return;
	}
	memcpy(temp,parent->children, parent->childrenCount * sizeof(TreeNode)); 
	parent->children = temp;
	parent->childrenCount = parent->childrenCount*CHILDREN_COUNT_MULTIPLIER;
}

TreeNode* insertChildren(TreeNode* parent, TreeNode* insertChildren, jvmtiEnv* jvmti){
	TreeNode* list = parent->children;
	jvmtiError error;
	int count = 0;
	while(list[count].isSet){
		//List is full
		if(count == parent->childrenCount){
			copyAndExtendChildrenList(parent, jvmti);
			break;	
		}
		count++;
	}
	memcpy(&(list[count]), insertChildren, sizeof(TreeNode));
	if ((error = (*jvmti)->Deallocate(jvmti, (unsigned char *)insertChildren)) != JVMTI_ERROR_NONE) {
		printf("Error deallocate inserted Tree Node\n");
	}
	list[count].isSet = TRUE;
	return &(list[count]);
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

int calcStackSum(TreeNode* children, int childrenCount){
	long sum = 0;
	int count = 0;	
	while(count < childrenCount && children[count].isSet){
		sum += children[count].method->count;
		count++;
	}
	return sum;
}

void calcSelfFrames(TreeNode* top){	
	TreeNode* children = top->children;
	int count = 0;
	long sum = 0;
	while(count < top->childrenCount && &(children[count]) != NULL && children[count].isSet){
		if(top->method != NULL){	
			sum = calcStackSum(children, top->childrenCount); 
			top->method->selfCount = top->method->count - sum;
		}
		calcSelfFrames(&(children[count]));
		count++;
	}
}

void convertFramesToTime(TreeNode* top){
	TreeNode* children = top->children;
	int count = 0;
	double factor = (endtime-starttime)/(((double)stackCount)*1000.0);	//µs
	factor /= 1000.0;							//ms
	if(top->method != NULL){	
		top->method->count = top->method->count * factor;
		top->method->selfCount = top->method->selfCount * factor;
	}
	while(count < top->childrenCount && &(children[count]) != NULL && children[count].isSet){
		convertFramesToTime(&(children[count]));
		count++;
	}
}

TreeNode* calcMaxSelf(TreeNode* top){	
	TreeNode* temp;
	TreeNode* childNode;
	int count = 0;	


	if(top->method != NULL && !(top->isTraversed)){
		temp = top;
	}
	if(top->method == NULL){
		temp = top->children;
	}

	while(count < top->childrenCount && &(top->children[count]) != NULL && top->children[count].isSet){
		childNode = calcMaxSelf(&(top->children[count]));
		if(temp == NULL || (childNode != NULL && childNode->method->selfCount >= temp->method->selfCount)){
			temp = childNode;
		}
		count++;
	}
	
	return temp;
}

void printHottestMethods(TreeNode* top){	
	TreeNode* children = top->children;
	TreeNode* hotMethod;
	TreeNode* parent; 
	int selfMax = 0;
	int count = 0;
	int hotCount = 1;
	while(hotCount <= 10){
		selfMax = 0;
		count = 0;
		hotMethod = calcMaxSelf(top);
		if(hotMethod != NULL){
			hotMethod->isTraversed = TRUE;
			printf("\n%s Self: %f ms Wall-Clock: %f ms\n", hotMethod->method->methodName, hotMethod->method->selfCount, hotMethod->method->count);
			parent = hotMethod->parent;
			printf("Parents: \n");
			while(parent->method != NULL){
				printf("\t %s Self: %f ms Wall-Clock: %f ms \n", parent->method->methodName, parent->method->selfCount, parent->method->count);
				parent = parent->parent;
			}
			printf("\n");
		}
		hotCount++;
	}
}

void JNICALL run(jvmtiEnv* jvmti, JNIEnv* jni, void* args) {
	jvmtiFrameInfo *frames;
	jvmtiStackInfo *stack_info;
	jint thread_count;
	char *methodName;
	jvmtiError error;
	int ti;
	int randSleepTime;

	stackCount = 0;
	if((error = (*jvmti)->Allocate(jvmti, sizeof(TreeNode), (unsigned char **)(&top)) != JVMTI_ERROR_NONE)){
		printf("Error allocating top tree node");
		return;
	}
	
	if((error = (*jvmti)->Allocate(jvmti, INITIAL_CHILDREN_COUNT * sizeof(TreeNode), (unsigned char **)&(top->children)) != JVMTI_ERROR_NONE)){
		printf("Error allocating treeNode children");
		return;
	}
	top->childrenCount = INITIAL_CHILDREN_COUNT;
	if ((error = (*jvmti)->GetTime(jvmti, &starttime)) != JVMTI_ERROR_NONE){
		printf("error start\n");
	}


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
				if((error = (*jvmti)->GetMethodName(jvmti, frames[fi].method, &methodName, NULL, NULL)) != JVMTI_ERROR_NONE) {
					break;
				}

				TreeNode* children = searchChildren(current, frames[fi].method);				
				if(children == NULL){
					MethodTiming* childMethod = createMethodTiming(frames[fi].method, methodName, jvmti);
					TreeNode* childNode = createTreeNode(childMethod, current, jvmti);
					current = insertChildren(current, childNode, jvmti);
				}
				else{
					children->method->count++;
					children->method->selfCount++;
					current = children;
				}
			}
		}
		
		if ((error = (*jvmti)->GetTime(jvmti, &endtime)) != JVMTI_ERROR_NONE){
			printf("error endtime\n");
		}
		if(sleepTime >= 0 ){
			usleep(sleepTime);	
		}else{
			randSleepTime = rand()%500;
			usleep(randSleepTime);
		}
	}
	
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
	srand(time(NULL));
	sleepTime = -1; 	

	jvmtiCapabilities requestedCapabilities, potentialCapabilities;
	memset(&requestedCapabilities, 0, sizeof(requestedCapabilities));
	if ((error = (*jvmti)->GetPotentialCapabilities(jvmti, &potentialCapabilities)) != JVMTI_ERROR_NONE) {
		return -1;
	}
	if ((error = (*jvmti)->AddCapabilities(jvmti, &requestedCapabilities)) != JVMTI_ERROR_NONE) {
		return -1;
	}

	jvmtiEventCallbacks callbacks;
	memset(&callbacks, 0, sizeof(callbacks));
	callbacks.VMInit = OnVMInit;
	(*jvmti)->SetEventCallbacks(jvmti, &callbacks, sizeof(callbacks));
	(*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, NULL);
	return 0;
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *_vm) {	
	printf("\nStartTime: %lu ms , EndTime: %lu ms", (long)starttime/1000000, (long)endtime/1000000);
	printf("\nTime: %lu ms", ((long)(endtime-starttime))/1000000);
	printf("\nRuntime: %lu ms \nStackCount: %d\n\n", (long) (endtime-starttime)/1000000, stackCount);
		
	calcSelfFrames(top);
	convertFramesToTime(top);
	printHottestMethods(top);	
}

