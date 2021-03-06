#include "stdafx.h"
#include <Windows.h>
#include "peBase.hpp"
#include "fixIAT.hpp"
#include "fixReloc.hpp"

bool peLoader(const char *exePath)
{
	LONGLONG fileSize = -1;
	BYTE *data = MapFileToMemory(exePath, fileSize);
	BYTE* pImageBase = NULL;
	LPVOID preferAddr = 0;
	IMAGE_NT_HEADERS32 *ntHeader = (IMAGE_NT_HEADERS32 *)getNtHdrs(data);
	if (!ntHeader) 
	{
		printf("[+] File %s isn't a 32bit PE file.", exePath);
		return false;
	}

	IMAGE_DATA_DIRECTORY* relocDir = getPeDir(data, IMAGE_DIRECTORY_ENTRY_BASERELOC);
	preferAddr = (LPVOID)ntHeader->OptionalHeader.ImageBase;
	printf("[+] Exe File Prefer Image Base at %x\n", preferAddr);

	pImageBase = (BYTE *)VirtualAlloc(preferAddr, ntHeader->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!pImageBase && !relocDir)
	{
		printf("[-] Allocate Image Base At %x Failure.\n", preferAddr);
		return false;
	}
	if (!pImageBase && relocDir)
	{
		printf("[+] Try to Allocate Memory for New Image Base\n");
		pImageBase = (BYTE *)VirtualAlloc(NULL, ntHeader->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (!pImageBase)
		{
			printf("[-] Allocate Memory For Image Base Failure.\n");
			return false;
		}
	}
	
	printf("[+] Mapping Section ...");
	ntHeader->OptionalHeader.ImageBase = (DWORD)pImageBase;
	memcpy(pImageBase, data, ntHeader->OptionalHeader.SizeOfHeaders);
	for (DWORD count = 0; count < ntHeader->FileHeader.NumberOfSections; count++)
	{
		IMAGE_SECTION_HEADER * SectionHeader = (IMAGE_SECTION_HEADER *)(
			DWORD(ntHeader) + sizeof(IMAGE_NT_HEADERS) + IMAGE_SIZEOF_SECTION_HEADER * count
		);
		printf("    [+] Mapping Section %s\n", SectionHeader->Name);
		memcpy
		(
			LPVOID(DWORD(pImageBase) + SectionHeader->VirtualAddress),
			LPVOID(DWORD(data) + SectionHeader->PointerToRawData),
			SectionHeader->SizeOfRawData
		);
	}
	fixIAT(pImageBase);

	if (pImageBase != preferAddr) 
		if (applyReloc((DWORD)pImageBase, (DWORD)preferAddr, pImageBase, ntHeader->OptionalHeader.SizeOfImage))
		puts("[+] Relocation Fixed.");
	DWORD retAddr = (DWORD)(pImageBase)+ntHeader->OptionalHeader.AddressOfEntryPoint;
	printf("Run Exe Module: %s\n", exePath);
	((void(*)())retAddr)();
}
#include <string>
int main(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("Usage: %s [Exe Path]", strrchr(argv[0], '\\') ? strrchr(argv[0], '\\') + 1 : argv[0]);
		getchar();
		return 0;
	}
	peLoader(argv[1]);
	getchar();
    return 0;
}

