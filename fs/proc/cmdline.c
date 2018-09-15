#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/setup.h>

static char updated_command_line[COMMAND_LINE_SIZE];

static void proc_cmdline_set(char *name, char *value)
{
	char *flag_pos, *flag_after;
	char *flag_pos_str = kmalloc(sizeof(char), COMMAND_LINE_SIZE);

	scnprintf(flag_pos_str, COMMAND_LINE_SIZE, "%s=", name);

	flag_pos = strstr(updated_command_line, flag_pos_str);
	if (flag_pos) {
		flag_after = strchr(flag_pos, ' ');
		if (!flag_after)
			flag_after = "";

		scnprintf(updated_command_line, COMMAND_LINE_SIZE, "%.*s%s=%s%s",
				(int)(flag_pos - updated_command_line),
				updated_command_line, name, value, flag_after);
	} else {
		// flag was not found, insert it
		scnprintf(updated_command_line, COMMAND_LINE_SIZE, "%s %s=%s", updated_command_line, name, value);
	}
}

static int cmdline_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", updated_command_line);
	return 0;
}

static int cmdline_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, cmdline_proc_show, NULL);
}

static const struct file_operations cmdline_proc_fops = {
	.open		= cmdline_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_cmdline_init(void)
{
	strcpy(updated_command_line, saved_command_line);
	if (strstr(updated_command_line, "androidboot.mode=usb_cable")) {
		proc_cmdline_set("androidboot.mode", "charger");
	}

	proc_create("cmdline", 0, NULL, &cmdline_proc_fops);
	return 0;
}
module_init(proc_cmdline_init);
