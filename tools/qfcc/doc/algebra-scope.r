typedef @algebra(float(3,0,1)) PGA;
typedef PGA.group_mask(0x0a) bivec_t;
typedef PGA.group_mask(0x1e) motor_t;
typedef PGA.tvec point_t;
typedef PGA.vec plane_t;
typedef bivec_t line_t;

@generic (genObj = [line_t, point_t]) {

vec2
intercept_times (line_t ray, genObj obj, float r)
{
	@algebra (PGA) {
		// Convert the Plücker coordinates to point+direction.
		//   Project the origin onto the line.
		auto rp = (e123 • ray * ~ray).tvec;
		//   Convert the direction of the line to a point at infinity.
		auto rv = e0 * ~ray;

		float uu = ray • ~ray;
		float vv = obj • ~obj;
		float s = uu * uu * vv;

		// rp + rv * t is a point on the ray.
		// Joining int with the object will produce a plane (for a line)
		// or a line (for a point) with a magnitude proportional to the
		// distance to the nearest point on the object.
		// The join operator is linear, so it can be distributed over the
		// sum allowing for t to be solved.
		auto a = (rp ∨ obj);
		auto b = (rv ∨ obj);
		float aa = a • ~a;
		float ab = a • ~b;
		float bb = b • ~b;
		float offs = sqrt (ab*ab - bb*(aa - s*r*r));
		auto ts = vec2 (-ab - offs, -ab + offs) / (bb * sqrt (uu));
		return ts;
	}
}

};
