--
-- GENie - Project generator tool
-- https://github.com/bkaradzic/GENie#license
--

local ninja = premake.ninja
local p = premake
local solution = p.solution

function ninja.generate_solution(sln)
	-- create a shortcut to the compiler interface
		local cc = premake[_OPTIONS.cc]

		-- build a list of supported target platforms that also includes a generic build
		local platforms = premake.filterplatforms(sln, cc.platforms, "Native")

		-- write a header showing the build options
		_p('# %s solution makefile autogenerated by GENie', premake.action.current().shortname)
		_p('# Type "make help" for usage help')
		_p('')
		_p('NINJA=ninja')

		-- set a default configuration
		_p('ifndef config')
		_p('  config=%s', _MAKE.esc(premake.getconfigname(sln.configurations[1], platforms[1], true)))
		_p('endif')
		_p('')

		local projects = table.extract(sln.projects, "name")
		table.sort(projects)

		_p('')
		_p('.PHONY: all clean help $(PROJECTS)')
		_p('')
		_p('all:')
		if (not sln.messageskip) or (not table.contains(sln.messageskip, "SkipBuildingMessage")) then
			_p(1, '@echo "==== Building all ($(config)) ===="')
		end
		_p(1, '@${NINJA} -C ${config} all')
		_p('')

		-- write the project build rules
		for _, prj in ipairs(sln.projects) do
			local prjx = ninja.get_proxy("prj", prj)
			_p('%s:', _MAKE.esc(prj.name))
			if (not sln.messageskip) or (not table.contains(sln.messageskip, "SkipBuildingMessage")) then
				_p(1, '@echo "==== Building %s ($(config)) ===="', prj.name)
			end
			_p(1, '@${NINJA} -C ${config} %s', prj.name)
			_p('')
		end

		-- clean rules
		_p('clean:')
		_p(1, '@${NINJA} -C ${config} -t clean')
		_p('')

		-- help rule
		_p('help:')
		_p(1,'@echo "Usage: make [config=name] [target]"')
		_p(1,'@echo ""')
		_p(1,'@echo "CONFIGURATIONS:"')

		local cfgpairs = { }
		for _, platform in ipairs(platforms) do
			for _, cfgname in ipairs(sln.configurations) do
				_p(1,'@echo "   %s"', premake.getconfigname(cfgname, platform, true))
			end
		end

		_p(1,'@echo ""')
		_p(1,'@echo "TARGETS:"')
		_p(1,'@echo "   all (default)"')
		_p(1,'@echo "   clean"')

		for _, prj in ipairs(sln.projects) do
			_p(1,'@echo "   %s"', prj.name)
		end

		_p(1,'@echo ""')
		_p(1,'@echo "For more information, see https://github.com/bkaradzic/genie"')
end

-- generate solution that will call ninja for projects
	local generate

	local function getconfigs(sln, name, plat)
		local cfgs = {}
		for prj in solution.eachproject(sln) do
			prj = ninja.get_proxy("prj", prj)
			for cfg in p.eachconfig(prj, plat) do
				if cfg.name == name then
					table.insert(cfgs, cfg)
				end
			end
		end
		return cfgs
	end

	function ninja.generate_ninja_builds(sln)
		-- create a shortcut to the compiler interface
		local cc = premake[_OPTIONS.cc]

		sln.getlocation = function(cfg, plat)
			return path.join(sln.location, premake.getconfigname(cfg, plat, true))
		end

		-- build a list of supported target platforms that also includes a generic build
		local platforms = premake.filterplatforms(sln, cc.platforms, "Native")

		for _,plat in ipairs(platforms) do
			for _,name in ipairs(sln.configurations) do
				p.generate(sln, ninja.get_solution_name(sln, name, plat), function(sln)
					generate(getconfigs(sln, name, plat))
				end)
			end
		end
	end

	function ninja.get_solution_name(sln, cfg, plat)
		return path.join(sln.getlocation(cfg, plat), "build.ninja")
	end

	function generate(prjcfgs)
		local cfgs          = {}
		local cfg_first     = nil
		local cfg_first_lib = nil

		_p("# solution build file")
		_p("# generated with GENie ninja")
		_p("")

		_p("# build projects")
		for _,cfg in ipairs(prjcfgs) do
			local key  = cfg.project.name

			-- fill list of output files
			if not cfgs[key] then cfgs[key] = "" end
			cfgs[key] = cfg:getoutputfilename() .. " "

			if not cfgs["all"] then cfgs["all"] = "" end
			cfgs["all"] = cfgs["all"] .. cfg:getoutputfilename() .. " "

			-- set first configuration name
			if (cfg_first == nil) and (cfg.kind == "ConsoleApp" or cfg.kind == "WindowedApp") then
				cfg_first = key
			end
			if (cfg_first_lib == nil) and (cfg.kind == "StaticLib" or cfg.kind == "SharedLib") then
				cfg_first_lib = key
			end

			-- include other ninja file
			_p("subninja " .. cfg:getprojectfilename())
		end

		_p("")

		_p("# targets")
		for cfg, outputs in iter.sortByKeys(cfgs) do
			_p("build " .. cfg .. ": phony " .. outputs)
		end
		_p("")

		_p("# default target")
		_p("default " .. (cfg_first or cfg_first_lib))
		_p("")
	end
