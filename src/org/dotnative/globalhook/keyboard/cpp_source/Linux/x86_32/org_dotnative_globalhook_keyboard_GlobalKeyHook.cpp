/*
 *	dotNative - GlobalKeyHook - 09/08/06
 *  Alex Barker
 *  
 *	JNI Interface for setting a Keyboard Hook and monitoring
 *	it with java.
 *
 *	Original windows code created by Jean-Francois Briere.  No license or 
 *  contact information was provided.
 *  http://www.velocityreviews.com/forums/showpost.php?p=1899928&postcount=5
 */

/*
This lib requires XEvie as XKeyGrap XKeyboard will not work.

emerge -av libXevie

Make sure to update the xorg.conf file extention section.
Section "Extensions"
    ...
    Option         "XEVIE" "Enable"
    ...
EndSection
*/

/*
Compiling options
g++ -m32 -march=i586 -shared -fPIC -lX11 -lXevie -I${JAVA_HOME}/include -I${JAVA_HOME}/include/linux ./org_dotnative_globalhook_keyboard_GlobalKeyHook.cpp -o libGlobalKeyListener.so
g++ -m64 -fPIC -o libGlobalKeyListener.so -march=i586 -shared -lX11 -lXevie -I/opt/sun-jdk-1.5.0.08/include -I/opt/sun-jdk-1.5.0.08/include/linux ./jni_keyboard_PollThread.cpp
*/

#ifdef DEBUG
#include <stdio.h>
#include <unistd.h>
#endif

#include <jni.h>
#include <stdlib.h>
#include <pthread.h>
#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/extensions/Xevie.h>
#include <X11/XKBlib.h>
#include <X11/Intrinsic.h>
#include "org_dotnative_globalhook_keyboard_GlobalKeyHook.h"

//#include <X11/Xutil.h>
//#include <X11/Xproto.h>

//Instance Variables
Display  *disp;
XEvent xev;
JavaVM * jvm = NULL;
jobject hookObj = NULL;
jmethodID fireKeyPressed_ID = NULL;
jmethodID fireKeyReleased_ID = NULL;
jmethodID fireKeyTyped_ID = NULL;
jclass objExceptionClass = NULL;
pthread_t hookThreadId = 0;

//Shared Object Constructor and Deconstructor
void __attribute__ ((constructor)) Init(void);
void __attribute__ ((destructor)) Cleanup(void);

void throwException(JNIEnv * env, char * sMessage) {
	if (objExceptionClass != NULL) {
		#ifdef DEBUG
		printf("C++ Exception: %s\n", sMessage);
		#endif
		
		env->ThrowNew(objExceptionClass, sMessage);
		//env->ExceptionDescribe();
		//env->DeleteLocalRef(objExceptionClass);
	}
	else {
		//Unable to find exception class
		
		#ifdef DEBUG
		printf("C++: Unable to locate exception class.\n");
		#endif
	}
}

void MsgLoop(JNIEnv * env) {
	while (TRUE) {
		XNextEvent(disp, &xev);
		
		//unsigned int iKeyCode = xev.xkey.keycode;
		unsigned int iKeyCode = XLookupKeysym(&xev.xkey, xev.xkey.state);
		unsigned int iKeyState = xev.xkey.state;
		unsigned long iKeyTime = xev.xkey.time;
		int iKeyType = xev.type;
		
		switch (iKeyType) {
			case KeyPress:
				#ifdef DEBUG
				printf("C++: MsgLoop - Key pressed\n");
				#endif
				
				env->CallVoidMethod(hookObj, fireKeyPressed_ID, (jlong) iKeyTime, (jint) iKeyType, (jint) iKeyCode, (jchar) char(iKeyCode));
			break;
			
			case KeyRelease:
				#ifdef DEBUG
				printf("C++: MsgLoop - Key released\n");
				#endif
				
				env->CallVoidMethod(hookObj, fireKeyReleased_ID, (jlong) iKeyTime, (jint) iKeyType, (jint) iKeyCode, (jchar) char(iKeyCode));
			break;
		}
		
		XevieSendEvent(disp, &xev, XEVIE_UNMODIFIED);
	}
}

JNIEXPORT void JNICALL Java_org_dotnative_globalhook_keyboard_GlobalKeyHook_registerHook(JNIEnv * env, jobject obj) {
	//Setup exception handleing
	objExceptionClass = env->FindClass("org/dotnative/globalhook/keyboard/GlobalKeyException");
	//objExceptionClass = (jclass) env->NewGlobalRef(env->FindClass("java/lang/Exception"));
	
	disp = XOpenDisplay(NULL);
	if (disp == NULL || TRUE) {
		//We couldnt hook a display so we need to die.
		char * str1 = "Could not open display";
		char * str2 = XDisplayName(NULL);
		char * str3 = (char *) calloc(strlen(str1) + strlen(str2) + 1, sizeof(char));
		
		strcat(str3, str1);
		strcat(str3, str2);
		
		throwException(env, str3);
		free(str3);
		
		//Naturaly exit so jni exception is thrown.
		return;
	}
	
	if(XevieStart(disp)) {
		#ifdef DEBUG
		printf("C++: XevieStart(disp) successful\n");
		#endif
	}
	else {
		//We had an error hooking the Xevie package.
		throwException(env, "XevieStart(disp) failed, only one client is allowed to do event interception.");
		
		//Naturaly exit so jni exception is thrown.
		return;
	}
	
	//Setup all the jni hook call back pointers.
	hookObj = env->NewGlobalRef(obj);
	jclass cls = env->GetObjectClass(hookObj);
	fireKeyPressed_ID = env->GetMethodID(cls, "fireKeyPressed", "(JIIC)V");
	fireKeyReleased_ID = env->GetMethodID(cls, "fireKeyReleased", "(JIIC)V");
	fireKeyTyped_ID = env->GetMethodID(cls, "fireKeyTyped", "(JIIC)V");
	env->GetJavaVM(&jvm);
	hookThreadId = pthread_self();
	
	//Set the mask for what we want to listen to with Xevie
	XevieSelectInput(disp, KeyPressMask | KeyReleaseMask);
	
	//Make sure we can detect when the button is being held down.
	XkbSetDetectableAutoRepeat(disp, TRUE, NULL);
	
	//Call listener
	MsgLoop(env);
}

JNIEXPORT void JNICALL Java_org_dotnative_globalhook_keyboard_GlobalKeyHook_unregisterHook(JNIEnv *env, jobject object) {
	XevieEnd(disp);
	#ifdef DEBUG
	printf("C++: XevieEnd(disp) successful\n");
	#endif
	
	pthread_cancel(hookThreadId);
	#ifdef DEBUG
	printf("C++: pthread_cancel(hookThreadId) successful\n");
	#endif
}

void Init() {
	//Do Notihing
	
	#ifdef DEBUG
	printf("C++: Init - Shared Object Process Attach.\n");
	#endif
}
 
void Cleanup() {
	#ifdef DEBUG
	printf("C++: Init - Shared Object Process Detach.\n");
	#endif
}