local started = os.clock()
local state = 17
local total = 0

for index = 0, 999999 do
    if index % 3 == 0 and state % 2 == 1 then
        state = (state * 5 + index) % 97
        total = total + state
    elseif index % 5 == 0 or state < 20 then
        state = (state + index % 11 + 7) % 97
        total = total - state
    else
        state = (state * 3 + 1) % 97
        if state > 60 then
            total = total + 3
        else
            total = total - 2
        end
    end
end

print(total + state)
print(math.floor((os.clock() - started) * 1e9 + 0.5))
