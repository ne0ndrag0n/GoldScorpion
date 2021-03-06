type Vec2u16
    x as u16
    y as u16

    function get() as Vec2u16
        return this
    end
end

def a as u16 = 0
def b as Vec2u16
def c as u8[ 64 ]

const LEET as u16 = 1330 + 7

@[interrupt=vblank]
function vblank()
    a = a + 1
end

function addVec( a1 as Vec2u16, b1 as Vec2u16 ) as Vec2u16
    def result as Vec2u16

    result.x = a1.x + b1.x
    result.y = a1.y + b1.y

	return result
end

a = (2 * 2) + (2 * 2)
