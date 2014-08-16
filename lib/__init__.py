import plutocracy.api


common = plutocracy.api.common
network = plutocracy.api.network
render = plutocracy.api.render
interface = plutocracy.api.interface
game = plutocracy.api.game

c_update = plutocracy.api.c_update
check_exit = plutocracy.api.check_exit

def init():
    """Sets everything up for the client"""
    
    common.register_variables()
    network.register_variables()
    render.register_variables()
    interface.register_variables()
    game.register_variables()
    common.parse_config_file("autogen.cfg")
    common.parse_config_file("autoexec.cfg")
    
    # Parse configuration scripts and open the log file
    interface.parse_config()
    common.open_log_file()
    # Run tests if they are enabled
    common.endian_check()
    common.test_mem_check()
    
    # Initialize
    common.init_lang()
    common.translate_vars()
    render.init_sdl()
    network.init()
    render.init()
    game.init()
    interface.init()
    render.load_test_assets()
    game.refresh_servers()
    print "init done"
    
def update():
    render.start_frame()
    interface.check_events()
    
    render.start_globe()
    game.render_globe();
    render.render_border(100)
    render.render_border(101)
    render.finish_globe()
    game.render_ships()
    game.render_game_over()
    
    interface.render();
    render.render_status();
    render.finish_frame();
    
    common.time_update();
    common.throttle_fps();

    # Update the game after rendering everything
    game.update_host();
    game.update_client();

def cleanup():
    common.cleanup()
    network.cleanup()
    render.cleanup()
    interface.cleanup()
    game.cleanup()
