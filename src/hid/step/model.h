
struct step_model_instance {
  const char *name;
  double x;
  double y;
  double rotation;
};


struct step_model {
  const char *filename;
  GList *instances;
  double ox;
  double oy;
  double oz;
  double ax;
  double ay;
  double az;
  double rx;
  double ry;
  double rz;
};

void export_step_assembly (const char *filename, GList *assembly_models);
