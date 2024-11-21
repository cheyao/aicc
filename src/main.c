#if !defined(OPENAI_API) || !defined(OPENAI_API_KEY)
_Static_assert(0, "Please set the API key");
#endif

#include <assert.h>
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

#include "cJSON.h"

struct memory {
	char* response;
	size_t size;
};

size_t write_data(void* buffer, size_t size, size_t nmemb, void* userp) {
	size_t realsize = size * nmemb;
	struct memory* mem = (struct memory*)userp;

	char* ptr = realloc(mem->response, mem->size + realsize + 1);
	if (!ptr) {
		return 0; /* out of memory */
	}

	mem->response = ptr;
	memcpy(&(mem->response[mem->size]), buffer, realsize);
	mem->size += realsize;
	mem->response[mem->size] = 0;

	return realsize;
}

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

	cJSON* data = cJSON_CreateObject();
	cJSON* model = cJSON_CreateString("gpt-4o");
	cJSON_AddItemToObject(data, "model", model);
	cJSON* messages = cJSON_CreateArray();
	cJSON_AddItemToObject(data, "messages", messages);
	cJSON* systemo = cJSON_CreateObject();
	cJSON_AddItemToArray(messages, systemo);

	cJSON* role1 = cJSON_CreateString("system");
	cJSON* content1 = cJSON_CreateString("You are a C compiler. Compile the C code to LLVM IR. "
					     "Do not return code blocks nor any comments.");
	cJSON_AddItemToObject(systemo, "role", role1);
	cJSON_AddItemToObject(systemo, "content", content1);

	cJSON* user = cJSON_CreateObject();
	cJSON_AddItemToArray(messages, user);

	cJSON* role2 = cJSON_CreateString("user");

	cJSON* content2 = cJSON_CreateString(buffer);
	cJSON_AddItemToObject(user, "role", role2);
	cJSON_AddItemToObject(user, "content", content2);

	char* sdata = cJSON_Print(data);

	curl_easy_setopt(curl, CURLOPT_URL, OPENAI_API);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, sdata);

	struct memory chunk = {0};
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);

	struct curl_slist* headers = NULL;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, "accept: application/json");
	headers = curl_slist_append(headers, "Authorization: Bearer " OPENAI_API_KEY);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	CURLcode success = curl_easy_perform(curl);

	if (success) {
		fprintf(stderr, "Curl fetch failed with error %d", success);

		return 1;
	}

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	cJSON_Delete(data);

	// Parse the response
	cJSON* response = cJSON_ParseWithLength(chunk.response, chunk.size);
	if (response == NULL) {
		const char* error_ptr = cJSON_GetErrorPtr();

		if (error_ptr != NULL) {
			fprintf(stderr, "Failed to parse API response json: %s\n", error_ptr);
		}

		return 1;
	}
	cJSON* choices = cJSON_GetObjectItemCaseSensitive(response, "choices");
	cJSON* choice;
	char* out = NULL;
	cJSON_ArrayForEach(choice, choices) {
		cJSON* message = cJSON_GetObjectItemCaseSensitive(choice, "message");
		cJSON* content = cJSON_GetObjectItemCaseSensitive(message, "content");
		out = content->valuestring;
		break;
	}

	// LLVM IR to C
	LLVMInitializeNativeTarget();
	LLVMInitializeNativeAsmPrinter();
	LLVMInitializeNativeAsmParser();
	LLVMContextRef context = LLVMContextCreate();

	LLVMModuleRef module = LLVMModuleCreateWithNameInContext("hardcoded_ir", context);
	char* errorMessage = NULL;

	// Parse the IR string
	if (LLVMParseIRInContext(context, LLVMCreateMemoryBufferWithMemoryRangeCopy(out, strlen(out), "hardcoded_ir"),
				 &module, &errorMessage) != 0) {
		fprintf(stderr, "Error parsing IR: %s\n", errorMessage);
		LLVMDisposeMessage(errorMessage);
		LLVMContextDispose(context);
		return 1;
	}

	free(chunk.response);

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
					LLVMCodeGenLevelDefault, LLVMRelocPIC, LLVMCodeModelDefault);

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
	snprintf(command, sizeof(command), "clang %s -o a.out -lm -lX11 -L/usr/X11R6/lib", outFname);
	if (system(command) != 0) {
		fprintf(stderr, "Error linking object file into executable.\n");
		return 1;
	}

	free(outFname);
	cJSON_Delete(response);

	printf("Successfully compiled binary. (a.out)\n");

	return 0;
}
