local byte = string.byte

local function lcs_length(left, right)
    local left_length = #left
    local right_length = #right
    local right_bytes = {}
    for index = 1, right_length do right_bytes[index] = byte(right, index) end
    local previous = {}
    local current = {}
    for index = 0, right_length do
        previous[index] = 0
        current[index] = 0
    end

    for i = 1, left_length do
        current[0] = 0
        local ch = byte(left, i)
        local diagonal, preceding = 0, 0
        for j = 1, right_length do
            local above = previous[j]
            if ch == right_bytes[j] then
                preceding = diagonal + 1
            elseif above > preceding then
                preceding = above
            end
            current[j] = preceding
            diagonal = above
        end
        previous, current = current, previous
    end
    return previous[right_length]
end

local left = "DYNAMICPROGRAMMINGALGORITHMSANDDATASTRUCTURESARECENTRALTOVIRTUALMACHINEBENCHMARKS"
local right = "ALGORITHMSFORDYNAMICLANGUAGESUSEPROGRAMSTRUCTUREANDRUNTIMEDATATOMEASUREPERFORMANCE"

local started = os.clock()
local result = 0
for _ = 1, 40 do
    result = result + lcs_length(left, right)
end
local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)

print(result)
print(elapsed)
