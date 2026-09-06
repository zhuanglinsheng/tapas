file(READ "${SOURCE_ROOT}/include/tapas/basic_defs/tbuiltintype.h" BUILTIN_HEADER)
file(READ "${SOURCE_ROOT}/include/tapas/basic_defs/tbasis.h" BASIS_HEADER)
file(READ "${SOURCE_ROOT}/src/runtime/tvm.c" VM_SOURCE)
file(READ "${SOURCE_ROOT}/src/runtime/tvm.h" VM_HEADER)
file(READ "${SOURCE_ROOT}/include/tapas/objects/trule_ir.h" RULE_IR_HEADER)

# Public header layout and dependency direction.
if(EXISTS "${SOURCE_ROOT}/include/tapas/runtime")
	message(FATAL_ERROR
		"legacy include/tapas/runtime directory reintroduced")
endif()
file(GLOB PUBLIC_COMPILER_HEADERS "${SOURCE_ROOT}/include/tapas/compile/*.h")
if(PUBLIC_COMPILER_HEADERS)
	message(FATAL_ERROR
		"internal compiler headers exposed under include/tapas/compile")
endif()
foreach(LEGACY_DSA_PATH IN ITEMS
		include/tapas/ds
		src/ds
		third_party)
	if(EXISTS "${SOURCE_ROOT}/${LEGACY_DSA_PATH}")
		message(FATAL_ERROR
			"legacy data-structure or dependency path remains: ${LEGACY_DSA_PATH}")
	endif()
endforeach()
if(NOT EXISTS "${SOURCE_ROOT}/src/dsa/third_party/pcg-c")
	message(FATAL_ERROR "dsa-local PCG dependency is missing")
endif()
foreach(REQUIRED_HEADER IN ITEMS
		include/tapas/objects/ttype.h
		include/tapas/objects/trule.h
		include/tapas/vm_service.h)
	if(NOT EXISTS "${SOURCE_ROOT}/${REQUIRED_HEADER}")
		message(FATAL_ERROR "required public header is missing: ${REQUIRED_HEADER}")
	endif()
endforeach()

foreach(PRIVATE_HEADER IN ITEMS
		include/tapas/tvm.h
		include/tapas/tenv.h
		include/tapas/tblas.h
		include/tapas/dsa/tblas.h
		include/tapas/dsa/tobj_array.h
		include/tapas/objects/tfunction_metadata.h
		include/tapas/cli/input_state.h
		include/tapas/lsp/server.h)
	if(EXISTS "${SOURCE_ROOT}/${PRIVATE_HEADER}")
		message(FATAL_ERROR
			"internal implementation header is public: ${PRIVATE_HEADER}")
	endif()
endforeach()

file(READ "${SOURCE_ROOT}/include/tapas/tsession.h" SESSION_HEADER)
if(SESSION_HEADER MATCHES "tapas/(tenv|tvm)\\.h")
	message(FATAL_ERROR
		"high-level tsession.h exposes concrete environment or VM headers")
endif()

file(READ "${SOURCE_ROOT}/include/tapas/vm_service.h" VM_SERVICES_HEADER)
if(VM_SERVICES_HEADER MATCHES "tapas/(tenv|tvm)\\.h")
	message(FATAL_ERROR
		"package-facing VM services expose concrete VM state")
endif()

file(READ "${SOURCE_ROOT}/include/tapas/objects/trule.h" RULE_HEADER)
if(RULE_HEADER MATCHES "tapas/(tenv|objects/trule_ir)\\.h")
	message(FATAL_ERROR
		"Rule object header imports execution state or RuleIR layout")
endif()

foreach(LIGHTWEIGHT_CONTAINER_HEADER IN ITEMS
		include/tapas/dsa/thashtbl.h
		include/tapas/dsa/tobj_vec.h)
	file(READ "${SOURCE_ROOT}/${LIGHTWEIGHT_CONTAINER_HEADER}"
		CONTAINER_HEADER)
	if(CONTAINER_HEADER MATCHES "tapas/(tbycs|tval)\\.h")
		message(FATAL_ERROR
			"container header has a heavyweight dependency: ${LIGHTWEIGHT_CONTAINER_HEADER}")
	endif()
endforeach()

foreach(REMOVED_DOMAIN_FILE IN ITEMS
		src/runtime/objects/tdomain.c
		include/tapas/objects/tdomain.h)
	if(EXISTS "${SOURCE_ROOT}/${REMOVED_DOMAIN_FILE}")
		message(FATAL_ERROR
			"rules Domain implementation remains in core: ${REMOVED_DOMAIN_FILE}")
	endif()
endforeach()
file(READ "${SOURCE_ROOT}/src/stdlib/rules/domain.c" RULES_DOMAIN_SOURCE)
if(NOT RULES_DOMAIN_SOURCE MATCHES "compo_extension")
	message(FATAL_ERROR "rules Domain is not registered as a package object")
endif()
foreach(CORE_DOMAIN_TOKEN IN ITEMS
		tdomain
		trules_domain
		compo_tpoints
		compo_trange
		tbuiltintype_points
		tbuiltintype_range
		ttype_kind_points
		ttype_kind_range
		tstatic_type_points
		tstatic_type_range)
	foreach(CORE_DOMAIN_FILE IN ITEMS
			include/tapas/basic_defs/tbuiltintype.h
			include/tapas/objects/ttype.h
			src/compile/types/static_type.h
			src/basic_defs/tbuiltintype.c
			src/runtime/objects/ttype.c
			src/compile/types/static_type.c
			src/compile/types/type_info.c
			src/compile/types/type_check.c
			src/runtime/objects/trule_ir.c
			src/runtime/objects/trule_term.c)
		file(READ "${SOURCE_ROOT}/${CORE_DOMAIN_FILE}" CORE_DOMAIN_SOURCE)
		if(CORE_DOMAIN_SOURCE MATCHES "${CORE_DOMAIN_TOKEN}")
			message(FATAL_ERROR
				"package Domain leaked into core: ${CORE_DOMAIN_FILE} uses ${CORE_DOMAIN_TOKEN}")
		endif()
	endforeach()
endforeach()

file(READ "${SOURCE_ROOT}/src/stdlib/rules/rules.c" RULES_PACKAGE_SOURCE)
file(READ "${SOURCE_ROOT}/src/stdlib/types/types.c" TYPES_PACKAGE_SOURCE)
foreach(CORE_RULE_TYPE IN ITEMS RuleIR RuleTerm RuleItem)
	if(RULES_PACKAGE_SOURCE MATCHES "\\.name[ \t]*=[ \t]*\"${CORE_RULE_TYPE}\"")
		message(FATAL_ERROR
			"rules package re-exports core object Type: ${CORE_RULE_TYPE}")
	endif()
	if(NOT TYPES_PACKAGE_SOURCE MATCHES
	   "\\.name[ \t]*=[ \t]*\"${CORE_RULE_TYPE}\"")
		message(FATAL_ERROR
			"types package does not expose core object Type: ${CORE_RULE_TYPE}")
	endif()
endforeach()
if(BASIS_HEADER MATCHES "compo_tpoints|compo_trange")
	message(FATAL_ERROR "package Domain object codes remain in the core ABI")
endif()
foreach(DOMAIN_CONSUMER IN ITEMS
		src/stdlib/types/types.c
		src/stdlib/solve/solve.c)
	file(READ "${SOURCE_ROOT}/${DOMAIN_CONSUMER}" DOMAIN_CONSUMER_SOURCE)
	if(DOMAIN_CONSUMER_SOURCE MATCHES
	   "domain\\.h|trules_domain")
		message(FATAL_ERROR
			"cross-package C dependency on rules Domain: ${DOMAIN_CONSUMER}")
	endif()
endforeach()
foreach(CORE_OBJECT IN ITEMS trule_ir trule_term trule_item)
	file(READ "${SOURCE_ROOT}/src/runtime/objects/${CORE_OBJECT}.c" CORE_OBJECT_SOURCE)
	if(NOT CORE_OBJECT_SOURCE MATCHES
	   "tcompo_vtable ${CORE_OBJECT}_vtable[	 ]*=")
		message(FATAL_ERROR
			"core object does not own its vtable: ${CORE_OBJECT}")
	endif()
	foreach(OTHER_OBJECT IN ITEMS trule_ir trule_term trule_item)
		if(NOT OTHER_OBJECT STREQUAL CORE_OBJECT AND CORE_OBJECT_SOURCE MATCHES
		   "tcompo_vtable ${OTHER_OBJECT}_vtable[	 ]*=")
			message(FATAL_ERROR
				"core object files mix vtables: ${CORE_OBJECT}, ${OTHER_OBJECT}")
		endif()
	endforeach()
	foreach(NON_OBJECT_OPERATION IN ITEMS
			trule_ir_semantic_hash
			trule_ir_content_hash
			trule_ir_serialize
			trule_ir_deserialize)
		if(CORE_OBJECT_SOURCE MATCHES "${NON_OBJECT_OPERATION}[	 ]*\\(")
			message(FATAL_ERROR
				"non-object Rule operation leaked into ${CORE_OBJECT}.c: ${NON_OBJECT_OPERATION}")
		endif()
	endforeach()
endforeach()

foreach(PACKAGE_RULE_OPERATION IN ITEMS
		trule_ir_semantic_hash
		trule_ir_content_hash
		trule_ir_serialize
		trule_ir_deserialize)
	if(RULE_IR_HEADER MATCHES "${PACKAGE_RULE_OPERATION}")
		message(FATAL_ERROR
			"rules package operation leaked into core header: ${PACKAGE_RULE_OPERATION}")
	endif()
endforeach()

if(EXISTS "${SOURCE_ROOT}/src/runtime/objects/trule_ir_transition.c")
	message(FATAL_ERROR
		"rules package operations remain in core objects")
endif()

foreach(REMOVED_RULE_FORMAT_FILE IN ITEMS
		src/runtime/objects/trule_format.c
		include/tapas/objects/trule_format.h)
	if(EXISTS "${SOURCE_ROOT}/${REMOVED_RULE_FORMAT_FILE}")
		message(FATAL_ERROR
			"central Rule object formatter remains: ${REMOVED_RULE_FORMAT_FILE}")
	endif()
endforeach()

file(READ "${SOURCE_ROOT}/src/runtime/tformat.c" GENERIC_FORMAT_SOURCE)
foreach(CONCRETE_FORMAT_DEPENDENCY IN ITEMS
		trule_ir
		trule_term
		trule_item
		trule_instance
		tdomain
		ttype_kind
		tfunction_metadata)
	if(GENERIC_FORMAT_SOURCE MATCHES "${CONCRETE_FORMAT_DEPENDENCY}")
		message(FATAL_ERROR
			"generic formatter knows concrete object: ${CONCRETE_FORMAT_DEPENDENCY}")
	endif()
endforeach()
if(GENERIC_FORMAT_SOURCE MATCHES
   "tformat_(type|function_signature)[\t ]*\\(")
	message(FATAL_ERROR
		"generic formatter owns concrete Type or function formatting")
endif()

file(READ "${SOURCE_ROOT}/src/runtime/objects/ttype.c" TYPE_OBJECT_SOURCE)
if(NOT TYPE_OBJECT_SOURCE MATCHES "tformat_type[\t ]*\\(")
	message(FATAL_ERROR "Type object does not own Type formatting")
endif()
file(READ "${SOURCE_ROOT}/src/runtime/objects/tfunction_metadata.c"
	FUNCTION_METADATA_SOURCE)
if(NOT FUNCTION_METADATA_SOURCE MATCHES
   "tformat_function_signature[\t ]*\\(")
	message(FATAL_ERROR
		"function metadata does not own signature formatting")
endif()

file(READ "${SOURCE_ROOT}/src/stdlib/rules/hash.c" RULES_HASH_SOURCE)
file(READ "${SOURCE_ROOT}/src/stdlib/rules/codec.c" RULES_CODEC_SOURCE)
foreach(HASH_OPERATION IN ITEMS
		trule_ir_semantic_hash
		trule_ir_content_hash)
	if(NOT RULES_HASH_SOURCE MATCHES "${HASH_OPERATION}[	 ]*\\(")
		message(FATAL_ERROR
			"rules package does not own hash operation: ${HASH_OPERATION}")
	endif()
endforeach()
foreach(CODEC_OPERATION IN ITEMS
		trule_ir_serialize
		trule_ir_deserialize)
	if(NOT RULES_CODEC_SOURCE MATCHES "${CODEC_OPERATION}[	 ]*\\(")
		message(FATAL_ERROR
			"rules package does not own codec operation: ${CODEC_OPERATION}")
	endif()
endforeach()

file(READ "${SOURCE_ROOT}/src/stdlib/evaluators/evaluators.c"
	EVALUATORS_SOURCE)
if(EVALUATORS_SOURCE MATCHES "trule_ir_(semantic|content)_hash")
	message(FATAL_ERROR
		"evaluators package depends directly on rules package hashing")
endif()

foreach(REMOVED_RULE_OBJECT_FILE IN ITEMS
		src/stdlib/rules/object.c
		src/stdlib/rules/object.h)
	if(EXISTS "${SOURCE_ROOT}/${REMOVED_RULE_OBJECT_FILE}")
		message(FATAL_ERROR
			"core Rule object remains in rules package: ${REMOVED_RULE_OBJECT_FILE}")
	endif()
endforeach()

foreach(FORBIDDEN IN ITEMS
        tbuiltintype_random
        tbuiltintype_evaluator
		 tbuiltintype_finite
        tbuiltintype_rule_origin
        tbuiltintype_rule_check_result
        tbuiltintype_rule_violation
        tbuiltintype_rule_diagnostic)
    if(BUILTIN_HEADER MATCHES "${FORBIDDEN}")
        message(FATAL_ERROR
            "Package-owned Type leaked into tbuiltintype_id: ${FORBIDDEN}")
    endif()
endforeach()

foreach(FORBIDDEN IN ITEMS
		"stdlib/"
		"evaluators::"
		"finite::"
		"rules::"
		"solve::"
		"tevaluator"
		"tfinite"
		"trandom"
		"tsolve")
	if(VM_SOURCE MATCHES "${FORBIDDEN}" OR VM_HEADER MATCHES "${FORBIDDEN}")
		message(FATAL_ERROR
			"Package implementation leaked into the VM: ${FORBIDDEN}")
	endif()
endforeach()

foreach(PACKAGE_SOURCE IN ITEMS
		src/stdlib/evaluators/evaluators.c
		src/stdlib/rules/rules.c
		src/stdlib/solve/solve.c)
	file(READ "${SOURCE_ROOT}/${PACKAGE_SOURCE}" PACKAGE_CONTENT)
	if(PACKAGE_CONTENT MATCHES "trule_builtin_new")
		message(FATAL_ERROR
			"Package dispatch leaked through a core builtin: ${PACKAGE_SOURCE}")
	endif()
endforeach()

foreach(FORBIDDEN IN ITEMS compo_trandom compo_tevaluator compo_tfinite)
    if(BASIS_HEADER MATCHES "${FORBIDDEN}")
        message(FATAL_ERROR
            "Package-owned object leaked into tcompo_type: ${FORBIDDEN}")
    endif()
endforeach()

foreach(REMOVED_CORE_FILE IN ITEMS
        src/runtime/objects/trandom.c
        src/runtime/objects/tevaluator.c
		src/runtime/objects/tfinite.c
		include/tapas/objects/trandom.h
		include/tapas/objects/tevaluator.h
		include/tapas/objects/tfinite.h
		include/tapas/objects/tsolve.h)
    if(EXISTS "${SOURCE_ROOT}/${REMOVED_CORE_FILE}")
        message(FATAL_ERROR
            "Package implementation remains in core: ${REMOVED_CORE_FILE}")
    endif()
endforeach()
