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
	for (i=0 ; i<[self count] ; i++)
	{
		if (strcasecmp (name, [[self objectAtIndex: i] classname]) < 0)
		{
			[self insertObject: ec atIndex:i];
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
	struct dirent **namelist, *ent;
	
	[self removeAllObjects];
	
     count = scandir(source_path, &namelist, NULL, NULL);
	
	for (i=0 ; i<count ; i++)
	{
		ent = namelist[i];
		if (strlen(ent->d_name) <= 3)
			continue;
		if (!strcmp (ent->d_name+strlen(ent->d_name)-3,".qc"))
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
	
	for (i=0 ; i<[self count] ; i++)
	{
		o = [self objectAtIndex: i];
		if (!strcmp (name,[o classname]) )
			return o;
	}
	
	return nullclass;
}


@end

