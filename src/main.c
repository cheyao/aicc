#if !defined(OPENAI_API) || !defined(OPENAI_API_KEY)
_Static_assert(0, "Please set the API key");
#endif

#include <curl/curl.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Core.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Support.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STRING(s) #s

int main(int argc, char* argv[]) {
	uint64_t fileCount = 0;
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] != '-') {
			fileCount++;
			continue;
		} else {
			printf("warning: flag %s ignored\n", argv[i]);
		}
	}

	if (fileCount == 0) {
		fprintf(stderr, "aicc: error: no input files\n");

		return 1;
	}

	char* buffer = 0;
	long length = 0;
	FILE* f = fopen(argv[1], "rb");

	if (!f) {
		fprintf(stderr, "Failed to open source file\n");
	}

	fseek(f, 0, SEEK_END);
	length = ftell(f);
	fseek(f, 0, SEEK_SET);
	buffer = malloc(length);
	if (!buffer) {
		fprintf(stderr, "Failed to aquire enough memory to read source file\n");
		return 1;
	}

	fread(buffer, 1, length, f);
	fclose(f);

	// AI to LLVM

	// Fetch result with curl
	curl_global_init(CURL_GLOBAL_ALL);
	CURL* curl = curl_easy_init();
	if (!curl) {
		fprintf(stderr, "Failed to fetch data with curl");

		free(buffer);

		return 1;
	}

	char* start =
		"{"
		"\"model\": \"gpt-4o-mini\""
		"\"messages\": \"[{\"role\": \"system\", \"content\": \"You are a compiler, compile the following "
		"C code into LLVM IR.\"}, {\"role\": \"user\", \"content\": \"";
	char* end = "\"}],\""
		    "}";
	char* data = malloc(strlen(start) + strlen(end) + strlen(buffer) + 1);

	if (!data) {
		free(buffer);

		fprintf(stderr, "Failed to allocate memory for payload\n");

		return 1;
	}

	strcpy(data, start);
	strcat(data, buffer);
	strcat(data, end);

	curl_easy_setopt(curl, CURLOPT_URL, STRING(OPENAI_API));
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &internal_struct);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);

	struct curl_slist* headers = NULL;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, "accept: application/json");
	headers = curl_slist_append(headers, "Authorization: Bearer " STRING(OPENAI_API_KEY));
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	CURLcode success = curl_easy_perform(curl);
	curl_slist_free_all(headers);

	curl_easy_cleanup(curl);

	free(buffer);
	free(data);

	// LLVM IR to C
	LLVMInitializeNativeTarget();
	LLVMInitializeNativeAsmPrinter();
	LLVMInitializeNativeAsmParser();
	LLVMContextRef context = LLVMContextCreate();

	LLVMModuleRef module = LLVMModuleCreateWithNameInContext("hardcoded_ir", context);
	char* errorMessage = NULL;

	// Parse the IR string
	if (LLVMParseIRInContext(context, LLVMCreateMemoryBufferWithMemoryRangeCopy(buffer, length, "hardcoded_ir"),
				 &module, &errorMessage) != 0) {
		fprintf(stderr, "Error parsing IR: %s\n", errorMessage);
		LLVMDisposeMessage(errorMessage);
		LLVMContextDispose(context);
		return 1;
	}

	// Validate the IR module
	if (LLVMVerifyModule(module, LLVMReturnStatusAction, &errorMessage) != 0) {
		fprintf(stderr, "Error verifying module: %s\n", errorMessage);
		LLVMDisposeMessage(errorMessage);
		LLVMDisposeModule(module);
		LLVMContextDispose(context);
		return 1;
	}

	// Get the target triple
	char* targetTriple = LLVMGetDefaultTargetTriple();
	LLVMSetTarget(module, targetTriple);

	// Get the target
	LLVMTargetRef target;
	if (LLVMGetTargetFromTriple(targetTriple, &target, &errorMessage) != 0) {
		fprintf(stderr, "Error getting target: %s\n", errorMessage);
		LLVMDisposeMessage(errorMessage);
		LLVMDisposeModule(module);
		LLVMContextDispose(context);
		return 1;
	}

	// Create the target machine
	LLVMTargetMachineRef targetMachine =
		LLVMCreateTargetMachine(target, targetTriple,
					"generic", // CPU
					"",	   // Features
					LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault);

	if (!targetMachine) {
		fprintf(stderr, "Error creating target machine.\n");
		LLVMDisposeModule(module);
		LLVMContextDispose(context);
		return 1;
	}

	// Emit the object file
	unsigned long len = strlen(argv[1]);
	char* outFname = malloc(len + 3);
	strcpy(outFname, argv[1]);
	outFname[len + 2] = 0;
	outFname[len + 1] = 'o';
	outFname[len] = '.';
	if (LLVMTargetMachineEmitToFile(targetMachine, module, outFname, LLVMObjectFile, &errorMessage) != 0) {
		fprintf(stderr, "Error emitting object file: %s\n", errorMessage);
		LLVMDisposeMessage(errorMessage);
		LLVMDisposeTargetMachine(targetMachine);
		LLVMDisposeModule(module);
		LLVMContextDispose(context);
		return 1;
	}

	printf("Object file written to %s\n", outFname);

	// Clean up
	LLVMDisposeTargetMachine(targetMachine);
	LLVMDisposeModule(module);
	LLVMContextDispose(context);
	LLVMDisposeMessage(targetTriple);

	// Use system command to link and create an executable
	char command[256];
	snprintf(command, sizeof(command), "clang %s -o a.out", outFname);
	if (system(command) != 0) {
		fprintf(stderr, "Error linking object file into executable.\n");
		return 1;
	}

	free(outFname);

	return 0;
}
