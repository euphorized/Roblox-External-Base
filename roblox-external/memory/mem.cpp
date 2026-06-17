#include "mem.h"
#include <iostream>
#include <vector>
#include "../offsets.h"

bool mem::read_buf(uintptr_t addr, void* buf, size_t size)
{
    if (!addr || !buf || !size) return false;
    SIZE_T got = 0;
    NTSTATUS s = NtReadVirtualMemory(h, (PVOID)addr, buf, size, &got);
    return s == 0 && got == size;
}

bool mem::write_buf(uintptr_t addr, const void* buf, size_t size)
{
    if (!addr || !buf || !size) return false;
    SIZE_T put = 0;
    NTSTATUS s = NtWriteVirtualMemory(h, (PVOID)addr, (PVOID)buf, size, &put);
    return s == 0 && put == size;
}

bool mem::rpm(uintptr_t addr, void* buf, size_t size)
{
    if (!addr || !buf || !size) return false;
    SIZE_T got = 0;
    return ReadProcessMemory(h, (LPCVOID)addr, buf, size, &got) && got == size;
}

bool mem::wpm(uintptr_t addr, const void* buf, size_t size)
{
    if (!addr || !buf || !size) return false;
    SIZE_T put = 0;
    return WriteProcessMemory(h, (LPVOID)addr, buf, size, &put) && put == size;
}

bool mem::attach(const wchar_t* name)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe =
    {
        sizeof(pe)
    };
    while (Process32NextW(snap, &pe))
        if (!wcscmp(pe.szExeFile, name))
        {
            pid = pe.th32ProcessID;
            break;
        }
    CloseHandle(snap);
    if (!pid)
        return false;
    h = OpenProcess(PROCESS_ALL_ACCESS, 0, pid);
    if (!h)
        return false;
    snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    MODULEENTRY32W me =
    {
        sizeof(me)
    };
    if (Module32FirstW(snap, &me))
        base = (uintptr_t)me.modBaseAddr;
    CloseHandle(snap);
    return base != 0;
}

std::string mem::read_string(uintptr_t addr)
{
    if (!addr || !is_valid(addr)) return "";
    int size = read<int>(addr + 0x10);
    if (size <= 0 || size > 255) return "";
    uintptr_t str_ptr = (size >= 16) ? read<uintptr_t>(addr) : addr;
    if (!str_ptr || !is_valid(str_ptr)) return "";
    char buf[256] = { 0 };
    NtReadVirtualMemory(h, (PVOID)str_ptr, buf, size, nullptr);
    return std::string(buf, size);
}

std::vector<uintptr_t> mem::get_children(uintptr_t parent)
{
    std::vector<uintptr_t> vec;
    if (!parent || !is_valid(parent)) return vec;

    uintptr_t child_ptr = read<uintptr_t>(parent + Offsets::Instance::ChildrenStart);
    if (!child_ptr || !is_valid(child_ptr)) return vec;

    uintptr_t start = read<uintptr_t>(child_ptr);
    uintptr_t end = read<uintptr_t>(child_ptr + Offsets::Instance::ChildrenEnd);

    if (!start || !end || !is_valid(start) || !is_valid(end)) return vec;
    if (start >= end) return vec;

    size_t diff = end - start;
    if (diff > 160000) return vec;

    size_t count = diff / 16;
    if (count > 10000) return vec;

    vec.reserve(count);

    for (uintptr_t i = start; i < end; i += 16)
    {
        uintptr_t child = read<uintptr_t>(i);
        if (child) vec.push_back(child);
        if (vec.size() >= 10000) break;
    }

    return vec;
}

uintptr_t mem::find_child(uintptr_t parent, const char* name)
{
    uintptr_t child_ptr = read<uintptr_t>(parent + Offsets::Instance::ChildrenStart);
    if (!child_ptr)
        return 0;
    uintptr_t start = read<uintptr_t>(child_ptr);
    uintptr_t end = read<uintptr_t>(child_ptr + sizeof(uintptr_t));
    for (uintptr_t i = start; i < end; i += 16)
    {
        uintptr_t child = read<uintptr_t>(i);
        if (!child)
            continue;
        uintptr_t name_ptr = read<uintptr_t>(child + Offsets::Instance::Name);
        std::string child_name = name_ptr ? read_string(name_ptr) : "";
        if (child_name == name)
            return child;
    }
    return 0;
}

uintptr_t mem::find_child_class(uintptr_t parent, const char* classname)
{
    uintptr_t child_ptr = read<uintptr_t>(parent + Offsets::Instance::ChildrenStart);
    if (!child_ptr)
        return 0;
    uintptr_t start = read<uintptr_t>(child_ptr);
    uintptr_t end = read<uintptr_t>(child_ptr + sizeof(uintptr_t));
    for (uintptr_t i = start; i < end; i += 16)
    {
        uintptr_t child = read<uintptr_t>(i);
        if (!child)
            continue;
        uintptr_t desc = read<uintptr_t>(child + Offsets::Instance::ClassDescriptor);
        if (!desc)
            continue;
        uintptr_t class_ptr = read<uintptr_t>(desc + Offsets::Instance::ClassName);
        if (!class_ptr)
            continue;
        std::string class_name = read_string(class_ptr);
        if (class_name == classname)
            return child;
    }
    return 0;
}

uintptr_t mem::allocate(size_t size)
{
    PVOID allocated = nullptr;
    auto result = NtAllocateVirtualMemory(h, &allocated, 0, &size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (result != 0)
    {
        printf("[-] allocation failed ( err code 0x%llx )\n", result);
        return 0;
    }
    return (uintptr_t)(allocated);
}

uintptr_t mem::find_first_child_of_class(uintptr_t instance, const std::string& classname)
{
    if (!instance || !is_valid(instance)) return 0;

    std::vector<uintptr_t> children = get_children(instance);
    for (uintptr_t child : children)
    {
        if (!child || !is_valid(child)) continue;
        uintptr_t class_descriptor = read<uintptr_t>(child + Offsets::Instance::ClassDescriptor);
        if (!class_descriptor || !is_valid(class_descriptor)) continue;
        uintptr_t class_name_ptr = read<uintptr_t>(class_descriptor + Offsets::Instance::ClassName);
        if (!class_name_ptr || !is_valid(class_name_ptr)) continue;
        std::string child_class = read_string(class_name_ptr);
        if (child_class == classname) return child;
    }
    return 0;
}

bool mem::is_valid(uintptr_t addr)
{
    if (!addr || addr < 0x10000 || addr > 0x7FFFFFFEFFFF)
        return false;

    return true;
}


bool mem::write_string(uintptr_t addr, std::string new_str)
{
    auto str = read<rbx_string>(addr);
    str.length = (uintptr_t)(new_str.length());

    if (str.length > 15)
    {
        if (new_str.length() > str.capacity)
        {
            while (new_str.length() > str.capacity)
            {
                str.capacity *= 2;
                str.capacity += 1;
            }
            str.data.pointer = allocate((size_t)(str.capacity));
        }
        write<rbx_string>(addr, str);
        NtWriteVirtualMemory(h, (PVOID)str.data.pointer, (PVOID)new_str.c_str(), new_str.length(), nullptr);
    }
    else
    {
        str.capacity = 15;
        memcpy(str.data.buffer, new_str.c_str(), new_str.length());
        if (new_str.length() < 16)
            str.data.buffer[new_str.length()] = '\0';
        write<rbx_string>(addr, str);
    }
    return true;
}

bool mem::protect(uintptr_t addr, size_t size, ULONG new_protect, ULONG* old_protect)
{
    PVOID base_addr = (PVOID)addr;
    SIZE_T region_size = size;
    ULONG old_prot = 0;

    auto result = NtProtectVirtualMemory(h, &base_addr, &region_size, new_protect, &old_prot);

    if (old_protect) {
        *old_protect = old_prot;
    }

    if (result != 0) {
        printf("[-] NtProtectVirtualMemory failed with status 0x%lx\n", result);
    }

    return result == 0;
}