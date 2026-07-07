**Conceptos Clave Previos**

*   **Acceso SSH:**  Deberás acceder a tu servidor Proxmox a través de SSH.  Asegúrate de que el servicio SSH esté habilitado y de que tengas las credenciales correctas (usuario y contraseña, o mejor aún, una clave SSH).
*   **Usuario Root o Sudo:**  La mayoría de los comandos de administración requieren privilegios de root.  Puedes usar el usuario `root` directamente (si está habilitado, lo cual no es recomendable por seguridad) o, preferiblemente, usar `sudo` delante de cada comando:  `sudo <comando>`.
*   **Nodos y Clústeres:** Proxmox puede funcionar como un solo servidor (nodo) o como un clúster de múltiples servidores.  Muchos comandos se aplican a nivel de nodo, pero otros (especialmente los relacionados con el clúster) tienen opciones para afectar a todo el clúster.
*   **IDs de VM/CT:** Las máquinas virtuales (VM) y los contenedores (CT) se identifican con un número único (VMID).  Este número es crucial para muchos comandos.
