

@interface TextureView: NSView
{
	id	parent_i;
	int	deselectIndex;
}

- setParent:(id)from;
- deselect;

@end
