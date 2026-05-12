set_project("coo")
set_version("1.0.0")

includes("extern")

set_languages("c17", "cxx23")

add_rules("mode.debug", "mode.release")

rule("slang2spv")
    set_extensions(".slang")

    on_load(function (target)
        local is_bin2c = target:extraconf("rules", "slang2spv", "bin2c")
        if is_bin2c then
            local headerdir = path.join(target:autogendir(), "rules", "slang2spv")
            if not os.isdir(headerdir) then
                os.mkdir(headerdir)
            end
            target:add("includedirs", headerdir)
        end
    end)
    
    before_buildcmd_file(function (target, batchcmds, sourcefile, opt)
        import("lib.detect.find_program")
        import("rules.utils.bin2c.utils", {alias = "bin2c_utils", rootdir = os.programdir()})

        -- slang to spv
        local profile = target:extraconf("rules", "slang2spv", "targetenv") or "spirv_1_4"
        local outputdir = target:extraconf("rules", "slang2spv", "outputdir") or path.join(target:autogendir(), "rules", "slang2spv")
        local spvfilepath = path.join(outputdir, path.filename(sourcefile) .. ".spv")
        batchcmds:show_progress(opt.progress, "${color.build.object}generating.slang2spv %s", sourcefile)
        batchcmds:mkdir(outputdir)
        batchcmds:vrunv("slangc", {
            path(sourcefile),
            "-target", "spirv",
            "-profile", profile,
            "-emit-spirv-directly",
            "-o", path(spvfilepath)})

        -- do bin2c or bin2obj
        local outputfile = spvfilepath
        local is_bin2c = target:extraconf("rules", "slang2spv", "bin2c")
        local is_bin2obj = target:extraconf("rules", "slang2spv", "bin2obj")
        if is_bin2c then
            -- generate header file
            -- note: explicitly disable zeroend (SPIR-V is binary format, not null-terminated string)
            local headerfile = bin2c_utils.generate_headerfile(target, batchcmds, spvfilepath, {
                progress = opt.progress,
                headerdir = outputdir,
                zeroend = false  -- SPIR-V is binary format, not null-terminated string
            })
            outputfile = headerfile
        elseif is_bin2obj then
            -- convert to object file using bin2obj
            -- note: zeroend is false by default (SPIR-V is binary format, not null-terminated string)
            local objectfile = bin2obj_utils.generate_objectfile(target, batchcmds, spvfilepath, {
                progress = opt.progress,
                rulename = "slang2spv"  -- pass current rule name for config lookup
            })
            outputfile = objectfile
        end

        -- add deps
        batchcmds:add_depfiles(sourcefile)
        batchcmds:set_depmtime(os.mtime(outputfile))
        batchcmds:set_depcache(target:dependfile(outputfile))
    end)
rule_end()

add_requires(
    "spdlog 1.17.0",
    "glfw 3.4",
    "glm 1.0.3",
    "taskflow 4.0.0",
    "vulkansdk")

target("test")
    set_policy("build.warning", true)
    set_warnings("all", "extra")

    set_kind("binary") 
    add_files("src/*.cpp")
    add_packages("glfw", "glm", "spdlog", "taskflow", "vulkansdk")
    add_deps("vma-hpp")

    add_defines(
        "VULKAN_HPP_NO_STRUCT_CONSTRUCTORS",
        "VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS")

    add_rules("slang2spv", {bin2c = true})
    add_files("src/*.slang")