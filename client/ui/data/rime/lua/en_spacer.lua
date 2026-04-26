-- 英文词条上屏自动添加空格
-- 在 engine/filters 的倒数第二个位置，增加 - lua_filter@*en_spacer
-- 
-- 英文后，再输入英文单词（必须为候选项）自动添加空格
local F = {}

function F.func( input, env )
    for cand in input:iter() do
        yield( cand )
    end
end

return F
