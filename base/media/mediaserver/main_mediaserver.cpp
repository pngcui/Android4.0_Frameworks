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
	//���һ��ProcessState��ʵ��
	/*
	*��/dev/binder���൱�ں��ں�binder�������˽�����ͨ��
	*ӳ��fd���ڴ棬�豸fd����ȥ������ڴ��binder�豸������write/read(fd)���൱��memcpy�Ĳ�����
	*/
    sp<ProcessState> proc(ProcessState::self());

	
	//�õ�һ��ServiceManager����
	/*
	*frameworks\base\libs\binder\IServiceManager.cpp
	*
	*ʵ���Ϸ��ص���BpServiceManager,����remote������BpBinder,�����handle������0
	*�������Ժ�ServiceManagerͨ��
	*/
    sp<IServiceManager> sm = defaultServiceManager();
    LOGI("ServiceManager: %p", sm.get());
	//��ʼ��AudioFlinger
    AudioFlinger::instantiate();

	
	//��ʼ��MediaPlayerService����
	/*
	*����BpServiceManager��addService����MediaPlayService��Ϣ���뵽ServiceManager��
	*ͨ��BnServiceManager(Service_manager.cpp�����������͸�binder�豸
	*/
    MediaPlayerService::instantiate();
    CameraService::instantiate();
    AudioPolicyService::instantiate();
	//����һ���̳߳�
    ProcessState::self()->startThreadPool();
	//�����������̳߳�
    IPCThreadState::self()->joinThreadPool();
}
