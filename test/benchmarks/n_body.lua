local pi = 3.141592653589793
local solar_mass = 4.0 * pi * pi
local days_per_year = 365.24

local function advance(bodies, dt)
    local count = #bodies
    for i = 1, count do
        local bi = bodies[i]
        for j = i + 1, count do
            local bj = bodies[j]
            local dx = bi.x - bj.x
            local dy = bi.y - bj.y
            local dz = bi.z - bj.z
            local d2 = dx * dx + dy * dy + dz * dz
            local mag = dt / (d2 * math.sqrt(d2))
            local mi = bi.mass * mag
            local mj = bj.mass * mag
            bi.vx = bi.vx - dx * mj
            bi.vy = bi.vy - dy * mj
            bi.vz = bi.vz - dz * mj
            bj.vx = bj.vx + dx * mi
            bj.vy = bj.vy + dy * mi
            bj.vz = bj.vz + dz * mi
        end
    end
    for i = 1, count do
        local b = bodies[i]
        b.x = b.x + dt * b.vx
        b.y = b.y + dt * b.vy
        b.z = b.z + dt * b.vz
    end
end

local function energy(bodies)
    local e = 0.0
    local count = #bodies
    for i = 1, count do
        local bi = bodies[i]
        e = e + 0.5 * bi.mass * (bi.vx * bi.vx + bi.vy * bi.vy + bi.vz * bi.vz)
        for j = i + 1, count do
            local bj = bodies[j]
            local dx = bi.x - bj.x
            local dy = bi.y - bj.y
            local dz = bi.z - bj.z
            e = e - bi.mass * bj.mass / math.sqrt(dx * dx + dy * dy + dz * dz)
        end
    end
    return e
end

local function offset_momentum(bodies)
    local px = 0.0
    local py = 0.0
    local pz = 0.0
    for i = 1, #bodies do
        local b = bodies[i]
        px = px + b.vx * b.mass
        py = py + b.vy * b.mass
        pz = pz + b.vz * b.mass
    end
    local sun = bodies[1]
    sun.vx = (0.0 - px) / solar_mass
    sun.vy = (0.0 - py) / solar_mass
    sun.vz = (0.0 - pz) / solar_mass
end

local bodies = {
    {
        x = 0.0,
        y = 0.0,
        z = 0.0,
        vx = 0.0,
        vy = 0.0,
        vz = 0.0,
        mass = solar_mass,
    },
    {
        x = 4.84143144246472090e+00,
        y = -1.16032004402742839e+00,
        z = -1.03622044471123109e-01,
        vx = 1.66007664274403694e-03 * days_per_year,
        vy = 7.69901118419740425e-03 * days_per_year,
        vz = -6.90460016972063023e-05 * days_per_year,
        mass = 9.54791938424326609e-04 * solar_mass,
    },
    {
        x = 8.34336671824457987e+00,
        y = 4.12479856412430479e+00,
        z = -4.03523417114321381e-01,
        vx = -2.76742510726862411e-03 * days_per_year,
        vy = 4.99852801234917238e-03 * days_per_year,
        vz = 2.30417297573763929e-05 * days_per_year,
        mass = 2.85885980666130812e-04 * solar_mass,
    },
    {
        x = 1.28943695621391310e+01,
        y = -1.51111514016986312e+01,
        z = -2.23307578892655734e-01,
        vx = 2.96460137564761618e-03 * days_per_year,
        vy = 2.37847173959480950e-03 * days_per_year,
        vz = -2.96589568540237556e-05 * days_per_year,
        mass = 4.36624404335156298e-05 * solar_mass,
    },
    {
        x = 1.53796971148509165e+01,
        y = -2.59193146099879641e+01,
        z = 1.79258772950371181e-01,
        vx = 2.68067772490389322e-03 * days_per_year,
        vy = 1.62824170038242295e-03 * days_per_year,
        vz = -9.51592254519715870e-05 * days_per_year,
        mass = 5.15138902046611451e-05 * solar_mass,
    },
}
local steps = 5000

offset_momentum(bodies)
local started = os.clock()
for step = 0, steps - 1 do
    advance(bodies, 0.01)
end
local result = math.floor((energy(bodies) + 200.0) * 1000000000.0)
local elapsed = math.floor((os.clock() - started) * 1e9 + 0.5)

print(result)
print(elapsed)
