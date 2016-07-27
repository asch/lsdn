function verify_env
{
	fail ping_from 1 4
}

function verify
{
	test ping_from 1 4
}
