/*
**
** Copyright 2008, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

// System headers required for setgroups, etc.
#include <sys/types.h>
#include <unistd.h>
#include <grp.h>

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <utils/Log.h>

#include <AudioFlinger.h>
#include <CameraService.h>
#include <MediaPlayerService.h>
#include <AudioPolicyService.h>
#include <private/android_filesystem_config.h>

using namespace android;

int main(int argc, char** argv)
{
	//获得一个ProcessState的实例
	/*
	*打开/dev/binder，相当于和内核binder机制有了交互的通道
	*映射fd到内存，设备fd传进去后，这块内存和binder设备共享，故write/read(fd)就相当于memcpy的操作了
	*/
    sp<ProcessState> proc(ProcessState::self());

	
	//得到一个ServiceManager对象
	/*
	*frameworks\base\libs\binder\IServiceManager.cpp
	*
	*实际上返回的是BpServiceManager,他的remote对象是BpBinder,传入的handle参数是0
	*表明可以和ServiceManager通信
	*/
    sp<IServiceManager> sm = defaultServiceManager();
    LOGI("ServiceManager: %p", sm.get());
	//初始化AudioFlinger
    AudioFlinger::instantiate();

	
	//初始化MediaPlayerService服务
	/*
	*调用BpServiceManager的addService，把MediaPlayService信息加入到ServiceManager中
	*通过BnServiceManager(Service_manager.cpp把命令请求发送给binder设备
	*/
    MediaPlayerService::instantiate();
    CameraService::instantiate();
    AudioPolicyService::instantiate();
	//启动一个线程池
    ProcessState::self()->startThreadPool();
	//加入启动的线程池
    IPCThreadState::self()->joinThreadPool();
}
