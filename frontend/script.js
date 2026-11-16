const API = "";
let currentStudent = null;
let adminLoggedIn = false;

const $ = (id) => document.getElementById(id);
function notify(message) {
  const toast = $("toast");
  toast.textContent = message;
  toast.classList.add("show");
  setTimeout(() => toast.classList.remove("show"), 3000);
}
async function api(path, options = {}) {
  const response = await fetch(API + path, {
    headers: { "Content-Type": "application/json", ...(options.headers || {}) },
    ...options
  });
  const data = await response.json();
  if (!response.ok || data.success === false) throw new Error(data.message || "Request failed");
  return data;
}
function escapeHTML(value) {
  return String(value ?? "").replace(/[&<>"']/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]));
}
function complaintCard(c, admin = false) {
  const statusClass = c.status === "Resolved" ? "resolved" : c.status === "In Progress" ? "progress" : "";
  return `<article class="complaint-item">
    <div class="complaint-top"><h4>#${c.id} · ${escapeHTML(c.category)}</h4><span class="badge ${statusClass}">${escapeHTML(c.status)}</span></div>
    <p><b>Location:</b> ${escapeHTML(c.location)} · <b>Student:</b> ${escapeHTML(c.usn)}</p>
    <p>${escapeHTML(c.description)}</p>
    <p class="muted">Submitted: ${escapeHTML(c.created)}</p>
    ${admin ? `<div class="status-control"><select id="status-${c.id}"><option ${c.status==="Pending"?"selected":""}>Pending</option><option ${c.status==="In Progress"?"selected":""}>In Progress</option><option ${c.status==="Resolved"?"selected":""}>Resolved</option></select><button class="button primary" onclick="updateStatus(${c.id})">Update status</button></div>` : ""}
  </article>`;
}
$("registerForm").addEventListener("submit", async e => {
  e.preventDefault();
  const payload = {name:$("studentName").value.trim(),usn:$("studentUSN").value.trim(),email:$("studentEmail").value.trim(),password:$("studentPassword").value};
  try { const r = await api("/api/register",{method:"POST",body:JSON.stringify(payload)}); notify(r.message); $("registerForm").reset(); }
  catch(err){ notify(err.message); }
});
$("loginForm").addEventListener("submit", async e => {
  e.preventDefault();
  try {
    const r = await api("/api/login",{method:"POST",body:JSON.stringify({usn:$("loginUSN").value.trim(),password:$("loginPassword").value})});
    currentStudent = r.student;
    $("studentWelcome").textContent = "Welcome, " + currentStudent.name;
    $("studentDashboard").classList.remove("hidden");
    $("studentDashboard").scrollIntoView({behavior:"smooth",block:"start"});
    $("loginForm").reset();
    await loadMyComplaints();
    notify("Student login successful.");
  } catch(err){ notify(err.message); }
});
$("studentLogout").addEventListener("click", () => {
  currentStudent = null; $("studentDashboard").classList.add("hidden"); notify("Logged out.");
});
$("complaintForm").addEventListener("submit", async e => {
  e.preventDefault();
  if (!currentStudent) return notify("Please log in first.");
  const payload = {usn:currentStudent.usn,category:$("category").value,location:$("location").value.trim(),description:$("description").value.trim()};
  try { const r = await api("/api/complaints",{method:"POST",body:JSON.stringify(payload)}); notify(r.message); $("complaintForm").reset(); await loadMyComplaints(); }
  catch(err){ notify(err.message); }
});
async function loadMyComplaints(){
  if(!currentStudent)return;
  try{
    const r=await api("/api/complaints?usn="+encodeURIComponent(currentStudent.usn));
    $("myComplaints").innerHTML=r.complaints.length?r.complaints.map(c=>complaintCard(c)).join(""):'<p class="muted">You have not submitted any complaints yet.</p>';
  }catch(err){notify(err.message);}
}
$("adminLoginForm").addEventListener("submit",async e=>{
  e.preventDefault();
  try{
    await api("/api/admin-login",{method:"POST",body:JSON.stringify({username:$("adminUsername").value,password:$("adminPassword").value})});
    adminLoggedIn=true;$("adminLoginPanel").classList.add("hidden");$("adminDashboard").classList.remove("hidden");
    await loadAllComplaints();notify("Admin login successful.");
  }catch(err){notify(err.message);}
});
$("adminLogout").addEventListener("click",()=>{
  adminLoggedIn=false;$("adminDashboard").classList.add("hidden");$("adminLoginPanel").classList.remove("hidden");notify("Admin logged out.");
});
async function loadAllComplaints(){
  if(!adminLoggedIn)return;
  try{
    const r=await api("/api/complaints");
    const list=r.complaints;
    $("totalCount").textContent=list.length;
    $("pendingCount").textContent=list.filter(c=>c.status==="Pending").length;
    $("progressCount").textContent=list.filter(c=>c.status==="In Progress").length;
    $("resolvedCount").textContent=list.filter(c=>c.status==="Resolved").length;
    $("allComplaints").innerHTML=list.length?list.map(c=>complaintCard(c,true)).join(""):'<p class="muted">No complaints have been submitted yet.</p>';
  }catch(err){notify(err.message);}
}
async function updateStatus(id){
  const status=$("status-"+id).value;
  try{const r=await api("/api/update-status",{method:"POST",body:JSON.stringify({id,status})});notify(r.message);await loadAllComplaints();}
  catch(err){notify(err.message);}
}
window.updateStatus=updateStatus;
