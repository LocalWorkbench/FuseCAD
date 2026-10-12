# End-to-end check of the CLI: create, inspect, re-parameterise and export.

# run(<expected exit code> <args>...): sets LAST_OUTPUT to the command's stdout.
function(run expected)
    execute_process(COMMAND ${CLI} ${ARGN} RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT rc EQUAL expected)
        message(FATAL_ERROR "fusecad-cli ${ARGN} exited with ${rc}, expected ${expected}:\n${out}\n${err}")
    endif()
    set(LAST_OUTPUT "${out}" PARENT_SCOPE)
endfunction()

set(doc ${WORKDIR}/smoke.fcad)
run(0 new ${doc} --sample plate)
run(0 info ${doc})
if(NOT LAST_OUTPUT MATCHES "Body1")
    message(FATAL_ERROR "info output is missing Body1:\n${LAST_OUTPUT}")
endif()
run(0 export ${doc} ${WORKDIR}/smoke.step -p width=100mm -p hole_d=12mm)
run(0 export ${doc} ${WORKDIR}/smoke.stl)
foreach(f smoke.step smoke.stl)
    if(NOT EXISTS ${WORKDIR}/${f})
        message(FATAL_ERROR "${f} was not written")
    endif()
endforeach()

# set persists the new value.
run(0 set ${doc} "width=120 mm")
run(0 info ${doc})
if(NOT LAST_OUTPUT MATCHES "width +=  *120 mm")
    message(FATAL_ERROR "set did not persist width:\n${LAST_OUTPUT}")
endif()

# A document whose recompute fails still prints, but exits with 2.
run(0 set ${doc} "corner_r=40 mm")
run(2 info ${doc})
if(NOT LAST_OUTPUT MATCHES "ERROR")
    message(FATAL_ERROR "info does not flag the failed feature:\n${LAST_OUTPUT}")
endif()
run(2 export ${doc} ${WORKDIR}/smoke-broken.step)

set(flange ${WORKDIR}/smoke-flange.fcad)
run(0 new ${flange} --sample flange)
run(0 export ${flange} ${WORKDIR}/smoke-flange.stl -p bolt_d=8mm)
set(block ${WORKDIR}/smoke-pillow-block.fcad)
run(0 new ${block} --sample pillow-block)
run(0 export ${block} ${WORKDIR}/smoke-pillow-block.step -p bore_d=36mm)

run(1)
run(1 frobnicate ${doc})
run(2 info ${WORKDIR}/missing.fcad)
