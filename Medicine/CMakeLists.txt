idf_component_register(SRCS "Medicine.c"
                    INCLUDE_DIRS "include"
                    REQUIRES Servo mqtt json
                    EMBED_TXTFILES "isrgrootx1.pem")
