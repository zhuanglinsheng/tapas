local function less(a, b)
    return a[1] < b[1] or (a[1] == b[1] and a[2] < b[2])
end
local function sift_down(queue, parent, size)
    local replacement = queue[parent]
    while parent * 2 <= size do
        local child = parent * 2
        if child < size and less(queue[child + 1], queue[child]) then child = child + 1 end
        if not less(queue[child], replacement) then break end
        queue[parent] = queue[child]
        parent = child
    end
    queue[parent] = replacement
end
local function pop(queue)
    local result = queue[1]
    local replacement = table.remove(queue)
    local size = #queue
    if size > 0 then
        local parent = 1
        while parent * 2 <= size do
            local child = parent * 2
            if child < size and less(queue[child + 1], queue[child]) then child = child + 1 end
            queue[parent] = queue[child]
            parent = child
        end
        while parent > 1 do
            local previous = parent // 2
            if not less(replacement, queue[previous]) then break end
            queue[parent] = queue[previous]
            parent = previous
        end
        queue[parent] = replacement
    end
    return result
end
local function bench(repetitions, values)
    local started = os.clock()
    local checksum = 0
    for _ = 1, repetitions do
        local queue = table.move(values, 1, #values, 1, {})
        for parent = #queue // 2, 1, -1 do sift_down(queue, parent, #queue) end
        for position = 1, #values do checksum = checksum + pop(queue)[1] * position end
    end
    local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)
    print(checksum)
    print(elapsed)
end
local values = {}
for i = 0, 255 do values[#values + 1] = {(i * 73 + 19) % 31 - 15, i} end
bench(64, values)
