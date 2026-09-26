# These drive commands through App's CommandProcessor to produce real AOF records.
add_server_test(testAofPersistence SERVER_APP)
add_server_test(testAofCompactor SERVER_APP)
