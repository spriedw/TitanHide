#include "hooks.h"
#include "hooklib.h"
#include "undocumented.h"
#include "ssdt.h"

static HOOK hNtQueryInformationProcess;
static HOOK hNtQueryObject;

ULONG GetProcessIDFromProcessHandle(HANDLE ProcessHandle)
{
    PROCESS_BASIC_INFORMATION PBI;
    if(NT_SUCCESS(ZwQueryInformationProcess(ProcessHandle, ProcessBasicInformation, &PBI, sizeof(PBI), NULL)))
        return PBI.UniqueProcessId;
    else
        return 0;
}

ULONG GetProcessIDFromThreadHandle(HANDLE ThreadHandle)
{
    typedef struct _THREAD_BASIC_INFORMATION
    {
        NTSTATUS ExitStatus;
        PVOID TebBaseAddress;
        CLIENT_ID ClientId;
        KAFFINITY AffinityMask;
        KPRIORITY Priority;
        KPRIORITY BasePriority;
    } THREAD_BASIC_INFORMATION, *PTHREAD_BASIC_INFORMATION;
    THREAD_BASIC_INFORMATION TBI;
    if(NT_SUCCESS(ZwQueryInformationThread(ThreadHandle, ThreadBasicInformation, &TBI, sizeof(TBI), NULL)))
        return (ULONG)TBI.ClientId.UniqueProcess;
    else
        return 0;
}

NTSTATUS HookNtQueryInformationProcess(
    IN HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    OUT PVOID ProcessInformation,
    IN ULONG ProcessInformationLength,
    OUT PULONG ReturnLength
)
{
    unhook(hNtQueryInformationProcess);
    NTSTATUS ret=NtQueryInformationProcess(ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength, ReturnLength);
    if(NT_SUCCESS(ret) && ProcessInformation)
    {
        ULONG hide=884;
        ULONG pid=GetProcessIDFromProcessHandle(ProcessHandle);
        if(ProcessInformationClass==ProcessDebugFlags)
        {
            DbgPrint("[TESTDRIVER] ProcessDebugFlags by %d\n", pid);
            if(pid==hide)
                *(unsigned int*)ProcessInformation=TRUE;
        }
        else if(ProcessInformationClass==ProcessDebugPort)
        {
            DbgPrint("[TESTDRIVER] ProcessDebugPort by %d\n", pid);
            if(hide==hide)
                *(unsigned int*)ProcessInformation=0;
        }
        else if(ProcessInformationClass==ProcessDebugObjectHandle)
        {
            DbgPrint("[TESTDRIVER] ProcessDebugObjectHandle by %d\n", pid);
            if(pid==hide)
                *(unsigned int*)ProcessInformation=0;
        }
    }
    hook(hNtQueryInformationProcess);
    return ret;
}

NTSTATUS HookNtQueryObject(
    IN HANDLE Handle OPTIONAL,
    IN OBJECT_INFORMATION_CLASS ObjectInformationClass,
    OUT PVOID ObjectInformation OPTIONAL,
    IN ULONG ObjectInformationLength,
    OUT PULONG ReturnLength OPTIONAL
)
{
    unhook(hNtQueryObject);
    NTSTATUS ret=NtQueryObject(Handle, ObjectInformationClass, ObjectInformation, ObjectInformationLength, ReturnLength);
    if(NT_SUCCESS(ret) && ObjectInformation)
    {
        ULONG pid=(ULONG)PsGetCurrentProcessId();
        ULONG hide=884;
        UNICODE_STRING DebugObject;
        RtlInitUnicodeString(&DebugObject, L"DebugObject");
        if(ObjectInformationClass==ObjectTypeInformation)
        {
            OBJECT_TYPE_INFORMATION* type=(OBJECT_TYPE_INFORMATION*)ObjectInformation;
            if(RtlEqualUnicodeString(&type->TypeName, &DebugObject, FALSE)) //DebugObject
            {
                DbgPrint("[TESTDRIVER] DebugObject by %d\n", pid);
                if(pid==hide)
                    type->TotalNumberOfObjects=0;
            }
        }
        else if(ObjectInformationClass==ObjectAllInformation)
        {
            OBJECT_ALL_INFORMATION* pObjectAllInfo=(OBJECT_ALL_INFORMATION*)ObjectInformation;
            unsigned char* pObjInfoLocation=(unsigned char*)pObjectAllInfo->ObjectTypeInformation;
            unsigned int TotalObjects=pObjectAllInfo->NumberOfObjects;
            for(unsigned int i=0; i<TotalObjects; i++)
            {
                OBJECT_TYPE_INFORMATION* pObjectTypeInfo=(OBJECT_TYPE_INFORMATION*)pObjInfoLocation;
                if(RtlEqualUnicodeString(&pObjectTypeInfo->TypeName, &DebugObject, FALSE)) //DebugObject
                {
                    DbgPrint("[TESTDRIVER] DebugObject by %d\n", pid);
                    if(pid==hide)
                        pObjectTypeInfo->TotalNumberOfObjects=0;
                }
                pObjInfoLocation=(unsigned char*)pObjectTypeInfo->TypeName.Buffer;
                pObjInfoLocation+=pObjectTypeInfo->TypeName.MaximumLength;
                duint tmp=((duint)pObjInfoLocation)&-sizeof(void*);
                if((duint)tmp!=(duint)pObjInfoLocation)
                    tmp+=sizeof(void*);
                pObjInfoLocation=((unsigned char*)tmp);
            }
        }
    }
    hook(hNtQueryObject);
    return ret;
}

bool HooksInit()
{
    hNtQueryInformationProcess=hook(L"NtQueryInformationProcess", (void*)HookNtQueryInformationProcess);
    if(!hNtQueryInformationProcess)
        return false;
    hNtQueryObject=hook(SSDTgpa("NtQueryObject"), (void*)HookNtQueryObject);
    if(!hNtQueryObject)
        return false;
    return true;
}

void HooksFree()
{
    unhook(hNtQueryInformationProcess, true);
    unhook(hNtQueryObject, true);
}
