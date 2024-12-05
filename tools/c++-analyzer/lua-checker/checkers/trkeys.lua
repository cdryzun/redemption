local utils = require'utils'

local pattern_trkey
do
    local peg = utils.peg
    local C = peg.C
    local P = peg.P
    local Ct = peg.Ct
    local After = peg.After

    pattern_trkey_def = Ct(After(
        P'  TR_KV' * P'_FMT'^-1 * '(' * C(peg.word)
    )^0)

    pattern_trkey = Ct(After(
        P'trkeys::' * C(peg.word)
    )^0)
end

local trkeys_path = 'src/translation/trkeys.hpp'

local match_and_setk = utils.match_and_setk

local keys

function init(args)
    keys = match_and_setk(pattern_trkey_def, utils.readall(trkeys_path), false)
end

function file(content)
    match_and_setk(pattern_trkey, content, true, keys)
end

function terminate()
    return utils.count_error(keys, "trkeys::%s not used")
end

return {init=init, file=file, terminate=terminate}
