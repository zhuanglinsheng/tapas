# Check generated input, actual decision and post-execution query in both modes.
foreach(example IN ITEMS test_generate_valid_exchange test_generate_rejected_exchange)
    set(source "${SOURCE_ROOT}/examples/retail/${example}.tap")
    execute_process(COMMAND "${TAPAS}" -c "${source}" RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${example} compilation failed")
    endif()
    foreach(mode IN ITEMS source bytecode)
        if(mode STREQUAL "source")
            set(command "${source}")
        else()
            set(command -e "${source}c")
        endif()
        execute_process(COMMAND "${TAPAS}" ${command}
            RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
        if(NOT result EQUAL 0 OR NOT output MATCHES "输入生成：sat"
            OR NOT output MATCHES "实际转移：sat"
            OR output MATCHES "unsupported|unknown|error")
            message(FATAL_ERROR "${example} ${mode} failed: ${output}${error}")
        endif()
        if(example STREQUAL "test_generate_valid_exchange")
            if(NOT output MATCHES "求出的支付额度（分）：100"
                OR NOT output MATCHES "模拟结果：成功"
                OR NOT output MATCHES "额度不超过 99 分且要求允许换货：unsat"
                OR NOT output MATCHES "payment_capacity in range")
                message(FATAL_ERROR "Acceptance/boundary generation failed: ${output}")
            endif()
        else()
            if(NOT output MATCHES "求出的支付额度（分）：99" OR NOT output MATCHES "模拟结果：拒绝")
                message(FATAL_ERROR "Rejection generation failed: ${output}")
            endif()
        endif()
    endforeach()
    file(REMOVE "${source}c")
endforeach()

set(source "${SOURCE_ROOT}/examples/retail/test_generate_joint_inputs.tap")
execute_process(COMMAND "${TAPAS}" -c "${source}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Joint-input compilation failed")
endif()
foreach(mode IN ITEMS source bytecode)
    if(mode STREQUAL "source")
        set(command "${source}")
    else()
        set(command -e "${source}c")
    endif()
    execute_process(COMMAND "${TAPAS}" ${command}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0 OR output MATCHES "unsupported|unknown|error")
        message(FATAL_ERROR "Joint-input ${mode} query failed: ${output}${error}")
    endif()
    foreach(expected IN ITEMS
        "支付额度：100；认证用户：yusuf_rossi_9620；确认目标：keyboard-clicky-plain"
        "支付额度：99；认证用户：yusuf_rossi_9620；确认目标：keyboard-clicky-plain"
        "支付额度：100；认证用户：yusuf_rossi_9620；确认目标：thermostat-google-home"
        "支付额度：100；认证用户：another_user；确认目标：keyboard-clicky-plain"
        "错误身份仍要求允许换货：unsat"
        "authenticated_user_id == 'another_user'")
        string(FIND "${output}" "${expected}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "Missing joint-input evidence ${expected}: ${output}")
        endif()
    endforeach()
    string(REGEX MATCHALL "模拟结果：成功；实际转移：sat" accepted "${output}")
    string(REGEX MATCHALL "模拟结果：拒绝；实际转移：sat" rejected "${output}")
    list(LENGTH accepted accepted_count)
    list(LENGTH rejected rejected_count)
    if(NOT accepted_count EQUAL 1 OR NOT rejected_count EQUAL 3)
        message(FATAL_ERROR "Joint-input execution coverage failed: ${output}")
    endif()
endforeach()
file(REMOVE "${source}c")
