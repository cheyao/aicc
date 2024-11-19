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
	// API URL is in OPENAI_API
	// API KEY is in OPENAI_API_KEY

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
	if (LLVMTargetMachineEmitToFile(targetMachine, module, outFname, LLVMObjectFile, &errorMessage) !=
	    0) {
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

	free(buffer);

	return 0;
}
