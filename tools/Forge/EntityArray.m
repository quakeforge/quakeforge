#import "qedefs.h"

@implementation EntityArray

/*
=================
insertEC:
=================
*/
- (void)insertEC: ec
{
	char	*name;
	int		i;
	
	name = [ec classname];
	for (i=0 ; i<numElements ; i++)
	{
		if (strcasecmp (name, [[self objectAt: i] classname]) < 0)
		{
			[self insertObject: ec at:i];
			return;
		}
	}
	[self addObject: ec];
}


/*
=================
scanFile
=================
*/
- (void)scanFile: (char *)filename
{
	int		size;
	char	*data;
	id		cl;
	int		i;
	char	path[1024];
	
	sprintf (path,"%s/%s", source_path, filename);
	
	size = LoadFile (path, (void *)&data);
	
	for (i=0 ; i<size ; i++)
		if (!strncmp(data+i, "/*QUAKED",8))
		{
			cl = [[EntityClass alloc] initFromText: data+i];
			if (cl)
				[self insertEC: cl];
			else
				printf ("Error parsing: %s in %s\n",debugname, filename);
		}
		
	free (data);
}


/*
=================
scanDirectory
=================
*/
- (void)scanDirectory
{
	int		count, i;
	struct direct **namelist, *ent;
	
	[self empty];
	
     count = scandir(source_path, &namelist, NULL, NULL);
	
	for (i=0 ; i<count ; i++)
	{
		ent = namelist[i];
		if (ent->d_namlen <= 3)
			continue;
		if (!strcmp (ent->d_name+ent->d_namlen-3,".qc"))
			[self scanFile: ent->d_name];
	}
}


id	entity_classes_i;


- initForSourceDirectory: (char *)path
{
	[super init];
	
	source_path = path;	
	[self scanDirectory];
	
	entity_classes_i = self;
	
	nullclass = [[EntityClass alloc] initFromText:
"/*QUAKED UNKNOWN_CLASS (0 0.5 0) ?"];

	return self;
}

- (id)classForName: (char *)name
{
	int		i;
	id		o;
	
	for (i=0 ; i<numElements ; i++)
	{
		o = [self objectAt: i];
		if (!strcmp (name,[o classname]) )
			return o;
	}
	
	return nullclass;
}


@end

