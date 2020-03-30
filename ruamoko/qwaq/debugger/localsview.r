#include <string.h>
#include <types.h>
#include "debugger/localsview.h"
#include "debugger/typeencodings.h"

@implementation LocalsView

+(LocalsView *)withRect:(Rect)rect target:(qdb_target_t)target
{
	return [[[self alloc] initWithRect:rect target:target] autorelease];
}

-initWithRect:(Rect)rect target:(qdb_target_t) target
{
	if (!(self = [super initWithRect:rect])) {
		return nil;
	}
	options = ofCanFocus;
	growMode = gfGrowHi;

	self.target = target;

	qdb_def_t   encodings_def = qdb_find_global (target, ".type_encodings");
	qdb_get_data (target, encodings_def.offset, sizeof(target_encodings),
				  &target_encodings);

	return self;
}

-(void)dealloc
{
	if (defs) {
		obj_free (defs);
		defs = nil;
	}
	if (data) {
		obj_free (data);
		data = nil;
	}
}

-setFunction:(unsigned) fnum
{
	if (current_fnum == fnum) {
		return self;
	}
	current_fnum =fnum;

	if (defs) {
		obj_free (defs);
		defs = nil;
	}
	if (data) {
		obj_free (data);
		data = nil;
	}
	func = qdb_get_function (target, fnum);
	aux_func = qdb_get_auxfunction (target, fnum);
	if (aux_func) {
		defs = qdb_get_local_defs (target, fnum);
	}
	if (func) {
		data = obj_malloc (func.local_size);
	}
	return self;
}

-draw
{
	[super draw];

	if (!data) {
		return self;
	}
	qdb_get_data (target, func.local_data, func.local_size, data);
	[self clear];
	if (!defs) {
		[self mvprintf:{0,0}, "%d", func.local_size];
		for (int y = 1; y < ylen; y++) {
			unsigned    ind = (y - 1) * 4;
			if (ind < func.local_size) {
				[self mvprintf:{0, y}, "%02x", ind];
				for (int x = 0; x < 4 && ind < func.local_size; x++, ind++) {
					[self mvprintf:{x * 9 + 3, y}, "%08x",
										  data[ind]];
				}
			} else {
				break;
			}
		}
	} else {
		for (int y = 0; y < ylen; y++) {
			if (y >= aux_func.num_locals) {
				break;
			}
			qdb_def_t *def = defs + y;
			unsigned    te = def.type_encoding + (int) target_encodings.types;
			qfot_type_t *type;
			type = [TypeEncodings getType:te fromTarget:target];
			[self mvprintf:{0, y}, "%s",
								  qdb_get_string (target, def.name)];
			@param      value = nil;
			string      valstr = "--";
			unsigned    offset = func.local_data + def.offset;
			qdb_get_data (target, offset, def.type_size >> 16, &value);
			switch (def.type_size & 0xffff) {
				case ev_void:
				case ev_invalid:
				case ev_type_count:
					break;
				case ev_string:
					valstr = qdb_get_string (target, value.integer_val);
					break;
				case ev_float:
					valstr = sprintf ("%.9g", value.float_val);
					break;
				case ev_vector:
					valstr = sprintf ("%.9v", value.vector_val);
					break;
				case ev_entity:
					valstr = sprintf ("%e", value.entity_val);
					break;
				case ev_field:
					valstr = sprintf ("[%x]", value.field_val);
					break;
				case ev_func:
					valstr = sprintf ("[%x]", value.func_val);
					break;
				case ev_pointer:
					valstr = sprintf ("[%x]", value.pointer_val);
					break;
				case ev_quat:
					valstr = sprintf ("[%q]", value.quaternion_val);
					break;
				case ev_integer:
					valstr = sprintf ("%d", value.integer_val);
					break;
				case ev_uinteger:
					valstr = sprintf ("%d", value.integer_val);
					break;
				case ev_short:
					valstr = sprintf ("%d", value.integer_val);
					break;
				case ev_double:
					valstr = sprintf ("%.17g", value.double_val);
					break;
			}
			int         x = xlen - strlen (valstr);
			[self mvaddstr:{x, y}, valstr];
		}
	}

	return self;
}

@end
