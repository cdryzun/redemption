#!/usr/bin/env lua
function load_mod_or_error(mod)
    local ok, mod_or_err = pcall(require, mod)
    if not ok then
        io.stderr:write(mod_or_err .. [[


Maybe ?

    apt install lua-argparse lua-lpeg

or

    apt install lua luarocks
    luarocks --local install argparse
    luarocks --local install lpeg
    eval "$(luarocks path)"

]])
    end
    return mod_or_err
end

local parser = load_mod_or_error'argparse'(arg[0], "Lua ReDemPtion source checker")

function append_config(args, _, xs)
  local t = args[_]
  if t[xs[1]] then
      t[xs[1]] = t[xs[1]] .. ',' .. xs[2]
  else
      t[xs[1]] = xs[2]
  end
end

parser:argument('sources', 'Paths of source files'):args'*'
parser:option('--checks', "Comma-separated list of globs with optional '-' prefix. Globs are processed in order of appearance in the list. Globs without '-' prefix add checks with matching names to the set, globs with the '-' prefix remove checks with matching names from the set of enabled checks.")
    :default'*'
parser:flag('--list-checks', "List all enabled checks and exit. Use with -checks=* to list all available checks")
parser:option('--configs', 'Checker arguments', {}):argname{'<check>','<arguments>'}
    :count('*'):args(2):action(append_config)

local args = parser:parse()

local list_checkers = {
    config = true,
    error = true,
    vcfg = true,
    printf = true,
    trkeys = true,
    acl_report = true,
}

-- insert current path for require()
-- @{
local wcd = arg[0]:match('.*/')
if wcd then
    package.path = wcd .. '?.lua;' .. package.path
end
-- @}

local utils = require'utils'

local checkers = {}
if args.checks then
    local unknown_checkers, kcheckers = {}, {}
    local all_checker = function()
        for name in pairs(list_checkers) do
            kcheckers[name] = true
        end
    end
    local enable_checker = function(name)
        kcheckers[name] = true
    end
    local disable_checker = function(name)
        kcheckers[name] = false
    end

    local lpeg = load_mod_or_error'lpeg'
    local ident = (1 - lpeg.S',\n ')^1
    local ws = lpeg.S'\n '^0
    local elem
        = lpeg.P'*' / all_checker
        + lpeg.P'-' * (ident / disable_checker)
        + ident / enable_checker
    local pattern = elem * (ws * ',' * ws * elem)^0 * ws * (',' * ws)^-1

    local r = pattern:match(args.checks)
    if not r or r <= #args.checks then
        error('--checks: invalid format')
    end

    local has_error = false
    for name,activated in pairs(kcheckers) do
        if not list_checkers[name] then
            utils.println_error('invalid checker: ' .. name)
            has_error = true
        end
        if activated then
            checkers[#checkers+1] = name
        end
    end
    if has_error then
        os.exit(1)
    end
end

if args.list_checks then
    for name in pairs(list_checkers) do
        print(name)
    end
    return
end


for i,checker in ipairs(checkers) do
    mod = require('checkers/' .. checker)
    if (mod.init(args.configs[checker]) or 0) > 0 then
        os.exit(1)
    end
    checkers[i] = {mod=mod, errcount=0}
end

function readall(fname)
    f,e = io.open(fname)
    if e then
        error(e)
    end
    local s = f:read('*a')
    f:close()
    return s
end

local contents
local readall = utils.readall
for _,filename in ipairs(args.sources) do
    content = readall(filename)
    for _,checker in ipairs(checkers) do
        checker.errcount = checker.errcount + (checker.mod.file(content, filename) or 0)
    end
end

local errcount = 0
for _,checker in ipairs(checkers) do
    if checker.errcount > 0 then
        errcount = errcount + checker.errcount
    else
        errcount = errcount + checker.mod.terminate()
    end
end
os.exit(math.min(errcount, 255))
