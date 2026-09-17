local byte = string.byte

local function lcs_length(left, right)
    local left_length = #left
    local right_length = #right
    local previous = {}
    local current = {}
    for index = 0, right_length do
        previous[index] = 0
        current[index] = 0
    end

    for i = 1, left_length do
        current[0] = 0
        for j = 1, right_length do
            if byte(left, i) == byte(right, j) then
                current[j] = previous[j - 1] + 1
            elseif previous[j] >= current[j - 1] then
                current[j] = previous[j]
            else
                current[j] = current[j - 1]
            end
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
