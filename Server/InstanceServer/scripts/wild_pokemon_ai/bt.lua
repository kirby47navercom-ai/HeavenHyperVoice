-- 각 상태의 작은 BT에서 공유하는 노드.
-- 트리는 명령을 선택하고 종료한다. 이동 경로와 타이머는 개체별 C++ 실행기가 유지한다.
local bt = {}

function bt.selector(children)
    return function(ctx)
        for _, child in ipairs(children) do
            local succeeded, command = child(ctx)
            if succeeded then
                return true, command
            end
        end
        return false, nil
    end
end

function bt.sequence(children)
    return function(ctx)
        local command
        for _, child in ipairs(children) do
            local succeeded, result = child(ctx)
            if not succeeded then
                return false, nil
            end
            command = result or command
        end
        return true, command
    end
end

function bt.condition(predicate)
    return function(ctx)
        return predicate(ctx), nil
    end
end

function bt.action(execute)
    return function(ctx)
        return true, execute(ctx)
    end
end

function bt.evaluate(root, ctx)
    local _, command = root(ctx)
    return command or { action = "wait" }
end

return bt
