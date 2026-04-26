local M = {}

M.setDbg = function(flg)
    return flg ~= nil
end

M.test = function(printPrefix)
    return printPrefix ~= nil
end

function M.init(...)
    return true
end

M.init()

return M
