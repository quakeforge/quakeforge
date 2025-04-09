#pragma bug die
typedef @algebra(float(3,0,0)) VGA;
typedef struct { VGA.bvec m[3]; } tensor_t;

tensor_t
inertia_tensor (VGA.vec *points)
{
	@algebra(VGA) {
		static VGA.bvec basis[3] = {e23, e31, e12};
		tensor_t tensor = nil;
		for (int k = 0; k < 3; k++) {
			VGA.bvec ten = nil;
			for (int i = 0; i < 4; i++) {
				for (int j = 0; j < 4; j++) {
					ten += (points[i] • points[j] * basis[k]
							- points[i] * basis[k] * points[j]).bvec
						   * ((i == j) ? 2 : 1);
				}
			}
			tensor.m[k] = ten;
		}
		return tensor;
	}
}
